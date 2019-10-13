/* 
    TO DO:
    > Don't accept ctrl+c from client as will shut down serv as well (maybe do as a client-side fix?)
        * Allow client to accept ctrl+c as this should shut it down.
*/

/*
    Required args: filename portno
    e.g. ./server 9999

    argv[0] filename
    argv[1] portno

*/


#define BUFFER_SIZE 2040
const int MAX_CLIENTS = 1;
const int MAX_ID_VAL = 255;
const int MIN_ID_VAL = 0;

int sockfd , newsockfd; // Socket file discriptors.
int servrun = 1;        // Server running state.
int streaming = 0;      // Whether streaming all messages.

#include <stdio.h>  // Standard input output - printf, scanf, etc.
#include <stdlib.h> // Contains var types, macros and some general functions (e.g atoi).
#include <string.h> // Contains string functionality, specifically string comparing.
#include <unistd.h> // Contains read, write and close functions.
#include <sys/types.h>  // Contain datatypes to do system calls (socket.h, in.h + more).
#include <sys/socket.h> // Definitions of structures for sockets (e.g. sockaddr struct).
#include <netinet/in.h> // Contain consts and structs needed for internet domain addresses.
#include <signal.h>

void StopServer()
{
    close(newsockfd);
    close(sockfd);
    exit(1);
}

void ExitState()
{
    if(streaming == 1)
        streaming = 0;    
    else
        StopServer();
}

// Outputs error to terminal and exits program.
void Error(const char *msg)
{
    perror(msg); 
    StopServer();
}


/* Signal Handler for SIGINT */
void sigintHandler(int sig_num) 
{ 
    signal(SIGINT, sigintHandler);       
    fflush(stdout); 
    fflush(stdin);
    StopServer();
} 



// Append str2 to end of str1.
char * str_append(char* str1, char* str2) 
{
    int n = strlen(str1) + strlen(str2);
    char* t_msg = calloc(n, sizeof(char));
    strcat(t_msg , str1);
    strcat(t_msg, str2);
    return t_msg;
}

// In str, replace first instance of 'old' string with 'new' one.
char * replace_str(char *str, char *old, char *new)
{
  static char buff[4096];
  char *p;

  if(!(p = strstr(str, old))) 
    return str;
   
  strncpy(buff, str, p-str); 
  buff[p-str] = '\0';
  sprintf(buff+(p-str), "%s%s", new, p+strlen(old));
  return buff;
}

// Return number of digits in integer.
int NumOfIntegerDigits(int number)
{
    if(number > 99) { return 3; }
    if(number > 9) { return 2; }    
    return 1;
}

// Convert integer to string.
char * ToCharArray(int number)
{
    int n = NumOfIntegerDigits(number);
    char *cArray = calloc(n, sizeof(char));
    sprintf(cArray, "%d" , number);
    return cArray;
}

// Write message to client and return if failed.
int WriteClient(int newsockfd, char* buffer) 
{    
    int n = write(newsockfd , buffer, strlen(buffer));
    return n;
}










////////////////////////////////////////////////////////////////////////////
long curMsgReadPriority = 0;
struct Message
{
    long readPriority;
    char msg[1024]; // Message containing 1024 characters.
};

struct Channel
{
    char id[3];         // Identifier of channel. Values of 0-255.
    int curSubPos;      // Point from where you can read future msgs.
    int curMsgReadPos;  // Cur point of reading.
    int totalMsgs;      // Total msgs in channel (readable and not).
    struct Message msgs[255];
};
struct Channel* allChans;

struct Client
{
    char id[3];
    int curChanPos; // Number of channel subscriptions.
    struct Channel channels[255];
} client;
////////////////////////////////////////////////////////////////////////////









/*
    Returns if channel ID is valid for use in commands.
    CONDITIONS:: Completely decimal and in value range of 0-255.
    INPUT:: Buffer containing id.
    OUTPUT:: Whether ID is valid for use in commands (0 good, -1 bad).
*/
int ValidID(char* id)
{ // TO DO: Fix single ASCII defaulting to 0 (e.g. SUB G == SUB 0).     
    int i_id = 0; // ID as converted integer.
    sscanf(id, "%d", &i_id); 
    
    if((strlen(id) - 1) > NumOfIntegerDigits(i_id)) // Integer shorter than string if have invalid char and convert. 
        return -1;

    if(i_id > MAX_ID_VAL || i_id < MIN_ID_VAL) // Fail if id outside allowed range.
        return -1; 
    return 0;
}





/*
    Return id from buffer.
    CONDITIONS:: N/A
    INPUT:: Buffer containing id, length of command and id char length.
    OUTPUT:: Valid id or bad id.
*/
char* GetIdFromBuffer(char buffer[BUFFER_SIZE], int cmdLen, int targetLen) // Gets ID from stdin buffer.
{    
    int len = cmdLen + 1; // CMD + space.    
    char* id = calloc(4, sizeof(char));
    memcpy(id , &buffer[len] , targetLen); // Get id from terminal entry.

    char* t_id = calloc(4, sizeof(char));
    for(int i = 0; i < 3; i++) // Get ID chars until hit space/end of id.
    {
        if(id[i] != '\0' && id[i] != ' ')
            t_id[i] = id[i];
        else
            break;
    }
    id = calloc(4, sizeof(char));
    strcpy(id, t_id);
    free(t_id);

    if(ValidID(id) == -1)
        return id;
  
    int i_id = atoi(id); // ID as converted integer.
    free(id);
    return ToCharArray(i_id);
}





/*
    Return msg contents of 1024 chars length from buffer.
    CONDITIONS:: Give ID as integer to calculate position of msg start.
    INPUT:: Buffer containing message and id as an integer.
    OUTPUT:: 1024 characters following the the channel id (from Send()).
*/
char* GetMsgFromBuffer(char buffer[BUFFER_SIZE], int i_id) 
{
    char* msg = calloc(1024, sizeof(char));
    int len = 4 + 2 + NumOfIntegerDigits(i_id); // CMD + (space * 2) + ID Len.    
    memcpy(msg , &buffer[len] , 1024); // Get id from terminal entry.
    return msg;
}





/*
    Return index or failure of searching for channel id in subs. 
    CONDITIONS:: Valid channel id, don't need to be subscribed to it.
    INPUT:: ID of target channel in buffer.
    OUTPUT:: Index of chan in subscriptions or -1 if not subbed to it.
*/
int GetClientSubIndex(char* id) // Return index of given channel.
{
    int n = client.curChanPos;
    for(int i = 0; i < n; i++)
    {
        if(strcmp(client.channels[i].id, id) == 0)
            return i;
    }
    return -1; // Failed to find.
}





/*
    Unsubscribe client from specified channel.
    CONDITIONS:: Valid id of channel subscribed to.
    INPUT:: ID of target channel in buffer.
    OUTPUT:: Confirmation of removing channel from subscriptions.
*/
char* Unsub(char buffer[BUFFER_SIZE]) // Unsubscribes client from specified channel using id.
{
    char* id = GetIdFromBuffer(buffer, 5, 4);   
    if(ValidID(id) < 0) // Invalid id.
        return replace_str("Invalid channel: xxx.\n","xxx",id);

    int i = GetClientSubIndex(id);
    if(i < 0) // Not subscribed to this chan id.
        return replace_str("Not subscribed to channel: xxx.\n","xxx",id);

    for(;i < client.curChanPos; i++)
    {
        strcpy(client.channels[i].id , client.channels[i+1].id);
        client.channels[i].curMsgReadPos = client.channels[i+1].curMsgReadPos;
        client.channels[i].totalMsgs = client.channels[i+1].totalMsgs;         
        memcpy(client.channels[i].msgs, client.channels[i+1].msgs, sizeof(client.channels[i].msgs));  
    }
    if(client.curChanPos > 0)
        client.curChanPos -= 1;    
    return replace_str("Usubscribed from channel: xxx.\n","xxx",id);
}





/*
    Add given channel to client subscriptions.
    CONDITIONS:: Valid id of integer value between 0-255.
    INPUT:: ID of target channel in buffer.
    OUTPUT:: Confirmation of adding channel to client subs.
*/
char* Sub(char buffer[BUFFER_SIZE])
{ // TO DO: Change Client Channels * to a "char* chans[3]" for efficency, also set it so starting read pos after last sent msg when sub.
    char* id = GetIdFromBuffer(buffer, 3, 4);

    if(ValidID(id) < 0) // Return if invalid id.
        return replace_str("Invalid channel: xxx.\n","xxx",id);

    if(GetClientSubIndex(id) >= 0)
        return replace_str("Already subscribed to channel xxx.\n","xxx",id);
    
    
    int i_id = atoi(id);

    // Set so when sub, start reading after latest msg.
    allChans[i_id].curSubPos = allChans[i_id].totalMsgs;
    allChans[i_id].curMsgReadPos = allChans[i_id].totalMsgs;
    
    // Add server info to client sub list (replace with char* arr[3] eventually).
    strcpy(client.channels[client.curChanPos].id, allChans[i_id].id);
    client.curChanPos++;    
    return replace_str("Subscribed to channel xxx.\n","xxx",id);
}





/*
    Saves client message to channel messages.
    CONDITIONS:: Valid chan ID and 1024 char msg length cutoff.
    INPUT:: Client to specify channel ID and message in buffer.
    OUTPUT:: Save message to server channel if valid and gives client outcome.
*/
char* Send(char buffer[BUFFER_SIZE]) 
{
    char* id = GetIdFromBuffer(buffer, 4, 4);
    int len = strlen(buffer) - 9;

    if(len > 1024) // limit msg length.
        len = 1024;
    if (len < 0)
        len = 0;

    if(ValidID(id) < 0) // Return if invalid id.
        return replace_str("Invalid channel: xxx.\n","xxx",id);

    int i_id = atoi(id);
    char* msg = GetMsgFromBuffer(buffer, i_id);
    strcpy(allChans[i_id].msgs[allChans[i_id].totalMsgs].msg, msg); // Add message to channel.
    
    // Assign read order priority to message if subbed to channel sent to.
    if(GetClientSubIndex(id) >= 0) {   
        allChans[i_id].msgs[allChans[i_id].totalMsgs].readPriority = curMsgReadPriority;
        curMsgReadPriority++;   
    } 

    allChans[i_id].totalMsgs++; // Increase total messages sent.
    return "";
}





/*
    Returns info on all subsribed channels to be sent to client.
    CONDITIONS:: Won't return anything if no subscriptions.
    INPUT:: N/A (uses global client subscription list).
    OUTPUT:: Formatted list of channels and their info as msg buffer.
*/
char* Channels()
{
    if(client.curChanPos <= 0)    
        return "";
        
    char *msg = calloc(BUFFER_SIZE , sizeof(char)); 
    strcpy(msg, "Channel Subscriptions\n"); 
    for(int i = 0; i < client.curChanPos; i++)
    {
        int i_id = atoi(client.channels[i].id); // Subbed channel[i] id as int.
        char* t_msg = "Channel: 111\tTotal Messages: 222\tRead: 333\tUnread 444\n"; 
        t_msg = replace_str(t_msg, "111", allChans[i_id].id); 
        t_msg = replace_str(t_msg, "222", ToCharArray(allChans[i_id].totalMsgs));
        t_msg = replace_str(t_msg, "333", ToCharArray(allChans[i_id].curMsgReadPos - allChans[i_id].curSubPos));
        t_msg = replace_str(t_msg, "444", ToCharArray(allChans[i_id].totalMsgs - allChans[i_id].curMsgReadPos));
        strcpy(msg, str_append(msg, t_msg)); // Append channel info to msg buffer.    
    }    
    return msg;
}





/*
    Gracefully terminates connection to client, leaving server online.
    CONDITIONS:: Client is connected to server.
    INPUT:: Socket id of client.
    OUTPUT:: N/A (removes all subscriptions and disconnects client).
*/
void Bye(int newsockfd)
{
    for(int i = client.curChanPos; i >= 0 ; i--) // Unsub from all cur channels.
    {
        char* b = calloc(BUFFER_SIZE, sizeof(char));
        strcpy(b, replace_str("UNSUB XXX\n", "XXX", client.channels[i].id));       
        Unsub(b);
    }
    strcpy(client.id, ToCharArray(atoi(client.id) + 1)); // Increment client id.
    close(newsockfd);
}





/*
    Returns next unread message of specified channel ID.
    CONDITIONS:: Client subbed to channel and has unread message.
    INPUT:: ID of channel to display message from in buffer.
    OUTPUT:: Next unread message.
*/
char* Next(char* buffer)
{
    char* id = GetIdFromBuffer(buffer, 4, 4);    
    if(ValidID(id) < 0)
        return replace_str("Invalid channel: XXX\n", "XXX", id);
    
    if(GetClientSubIndex(id) < 0)
        return replace_str("Not subscribed to channel XXX\n", "XXX", id);
    
    if(allChans[atoi(id)].totalMsgs - allChans[atoi(id)].curMsgReadPos > 0)
    {
        allChans[atoi(id)].curMsgReadPos++; 
        return allChans[atoi(id)].msgs[allChans[atoi(id)].curMsgReadPos - 1].msg;
    }
    return "";
}





/*
    Returns next unread message of any channel using FIFO.
    CONDITIONS:: If subbed to channel, if unread messages & order sent.
    INPUT:: N/A (checks IDs of subs to see which unread message to read first).
    OUTPUT:: Oldest readable message in any subscribed server.
*/
char* NextAll()
{
    if(client.curChanPos == 0) // If not return min length, no subs.
        return "Not subscribed to any channels.\n";

    int nextChanID = 99999;
    long nextMsgRdPriority = 99999;
   
    for(int i = 0; i < client.curChanPos; i++) // Loop through subscriptions.
    {
        if(allChans[atoi(client.channels[i].id)].curMsgReadPos < allChans[atoi(client.channels[i].id)].totalMsgs)
        {
            int readPos = allChans[atoi(client.channels[i].id)].curMsgReadPos;
            if(allChans[atoi(client.channels[i].id)].msgs[readPos].readPriority < nextMsgRdPriority) // Check if msg higher priority.
            {
                nextChanID = atoi(client.channels[i].id);
                nextMsgRdPriority = allChans[atoi(client.channels[i].id)].msgs[readPos].readPriority;   
            }           
        } 
    }

    if(nextChanID >= 99999) // No new messages, return nothing.
        return "";
    
    curMsgReadPriority = nextMsgRdPriority + 1;
    allChans[nextChanID].curMsgReadPos++;
    return str_append(str_append(ToCharArray(nextChanID),":"), allChans[nextChanID].msgs[allChans[nextChanID].curMsgReadPos-1].msg);
}





//
void LiveStream(int newsockfd, char buffer[BUFFER_SIZE])
{
    streaming = 1;
    char* newBuffer = calloc(BUFFER_SIZE, sizeof(char));
    
    if(strlen(buffer) >= 12) // If included an chan ID in cmd, construct as Next ID cmd.
        strcpy(newBuffer, replace_str(buffer, "LIVESTREAM", "NEXT"));
    while(streaming == 1)
    {
        if(strlen(newBuffer) > 0)
        {
            char* msg = Next(newBuffer);
            if(strlen(msg) > 0)            
                WriteClient(newsockfd, msg); 
        }
        else
        {
            char* msg = NextAll();
            if(strlen(msg) > 0)            
                WriteClient(newsockfd, msg); 
        }
    }
    return;
}








/*
    Main logic.
    Sets up server and beings passively listening.
    Actively listen for client commands and run relevant responses.
    If fail to read/write to client, try reconnect.
*/
int main(int argc, char * argv[])
{
    //  signal(SIGINT, sig_handler);
    int portno, n;
    char buffer[BUFFER_SIZE]; 

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen; // Sock addr size.
    
    bzero((char *) &serv_addr , sizeof(serv_addr)); // Clear any data in reference (make sure serv_addr empty).
    sockfd = socket(AF_INET, SOCK_STREAM, 0);       // IPv4, TCP, default to TCP.
    
    if(sockfd < 0) Error("Failed to open requested socket!\n");
    if(argc < 2) portno = 12345; // Default port if not specified.
    else portno = atoi(argv[1]); // Custom port number.

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); 

  
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        Error("Failed to bind socket address to memory!\n");
    
    listen(sockfd , MAX_CLIENTS); // Listen on socket passively.
    clilen = sizeof(cli_addr);    

    newsockfd = accept(sockfd , (struct sockaddr *) &cli_addr , &clilen); // Accept client connection (waits until complete).
    if(newsockfd < 0) 
        Error("Client failed to connect to server. Something is wrong!\n"); 
  
 
    allChans = calloc(255, sizeof(struct Channel)); // Create all channels.   
    for(int i = 0; i < 255; i++) {                  // Populate all channels with default values.
        strcpy(allChans[i].id, ToCharArray(i));
        allChans[i].curMsgReadPos = 0;
        allChans[i].totalMsgs = 0;
        allChans[i].curSubPos = 0;
    }   

    // Client setup & initialize first connection.
    client.curChanPos = 0;
    strcpy(client.id, "1"); 
    WriteClient(newsockfd, replace_str("Welcome! Your client ID is xxx.\n","xxx",client.id));
    
    while(servrun == 1)
    {
        bzero(buffer , BUFFER_SIZE); // Clear buffer.
        n = read(newsockfd , buffer , BUFFER_SIZE); // Wait point, wait for client to write.
        
        if(n < 0){ // On read fail, wait to reconnect.        
            newsockfd = accept(sockfd , (struct sockaddr *) &cli_addr , &clilen);
            WriteClient(newsockfd, replace_str("Welcome! Your client ID is xxx.\n","xxx",client.id));
        }

        char* msg;
        if(strstr(buffer , "UNSUB") != NULL)
            strcpy(msg , Unsub(buffer));          
        else if(strstr(buffer , "SUB") != NULL)                
            strcpy(msg , Sub(buffer)); 
        else if(strstr(buffer , "CHANNELS") != NULL)
            strcpy(msg, Channels());
        else if(strstr(buffer , "SEND") != NULL)
            strcpy(msg, Send(buffer));
        else if(strstr(buffer , "BYE") != NULL)            
            Bye(newsockfd);
        else if(strstr(buffer , "NEXT") != NULL)  
            if(strlen(buffer) >= 6)  
                strcpy(msg , Next(buffer));
            else
                strcpy(msg , NextAll());
        else
            strcpy(msg , "");
        if(strstr(buffer, "LIVESTREAM") != NULL)  
            LiveStream(newsockfd, buffer);
        if(strstr(buffer , "[+Hide-] EXEC EXIT SERVER") != NULL)
            StopServer();
        
        
                  
      
        int n = WriteClient(newsockfd, str_append("​", msg));   
        if(n < 0) // On write fail, wait to reconnect.
        { 
            newsockfd = accept(sockfd , (struct sockaddr *) &cli_addr , &clilen); 
            WriteClient(newsockfd, replace_str("Welcome! Your client ID is xxx.\n","xxx",client.id));
        }
    }
    close(newsockfd);
    close(sockfd);    
    return 0;
}