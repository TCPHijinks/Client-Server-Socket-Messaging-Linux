/*
    Required args: filename portno
    e.g. ./server 9999

    argv[0] filename
    argv[1] portno (optional)

*/
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>  // Standard input output - printf, scanf, etc.
#include <stdlib.h> // Contains var types, macros and some general functions (e.g atoi).
#include <string.h> // Contains string functionality, specifically string comparing.
#include <strings.h>
#include <unistd.h> // Contains read, write and close functions.
#include <sys/types.h>  // Contain datatypes to do system calls (socket.h, in.h + more).
#include <sys/socket.h> // Definitions of structures for sockets (e.g. sockaddr struct).
#include <netinet/in.h> // Contain consts and structs needed for internet domain addresses.
#include <signal.h>
#include <semaphore.h> 
#include <time.h>
int sockfd , newsockfd; // Socket file discriptors.
int portno, n;

const int MAX_CLIENTS = 4;




int streaming = 0;
#define BUFFER_SIZE 2040
#define DEFAULT_PORT 12345
const int MAX_ID_VAL = 256;
const int MIN_ID_VAL = 0;
int servrun = 1;        // Server running state.




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

struct Message
{
    long long timeSent;
    char msg[1024]; // Message containing 1024 characters.
};

struct Channel
{
    char id[3];         // Identifier of channel. Values of 0-255.
    int curSubPos;      // Point from where you can read future msgs.
    int curMsgReadPos;  // Cur point of reading.
    int totalMsgs;      // Total msgs in channel (readable and not).
    struct Message msgs[256];
};


struct Client
{
    char id[3];
    int curChanPos; // Number of channel subscriptions.
    struct Channel channels[256];
} client;
////////////////////////////////////////////////////////////////////////////



int shmid[3];
int *shm_cliForks = 0;    // Number of client forks used to assign client ids (Shared Mem).
struct Channel *allChans; // Global channels (Shared Mem).
sem_t *chanMutex;  // Channel mutex locks (Shared Mem).







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
    Wait until process can read/write to critical resource.
    CONDITIONS:: Whether the process is reading or writing to the chan.
    INPUT:: Valid channel id and if only reading from channel.
    OUTPUT:: N/A - waits until its turn to read/write channel.
*/
void CriticalChanAccess(int chanID, int reading)
{
    if(reading) // Read if not writing & allow immediate access to others.    ///////////////////////////////////////////////////////////
    {
        sem_wait(&chanMutex[chanID]);
        sem_post(&chanMutex[chanID]);
    }
    else // Wait until available to write.
    {
        sem_wait(&chanMutex[chanID]);
    }    
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

    for(;i < client.curChanPos; i++) // Overwrite unsubbed chan and decrement index positions of chans after it.
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
{ 
    char* id = GetIdFromBuffer(buffer, 3, 4);
    if(ValidID(id) < 0) // Return if invalid id.
        return replace_str("Invalid channel: xxx.\n","xxx",id);

    if(GetClientSubIndex(id) >= 0)
        return replace_str("Already subscribed to channel xxx.\n","xxx",id);
    int i_id = atoi(id);
    
    // Add server info to client sub list.
    strcpy(client.channels[client.curChanPos].id, allChans[i_id].id);

    // Client start reading after last msg before subscribed.
    client.channels[client.curChanPos].curSubPos = allChans[i_id].totalMsgs;
    client.channels[client.curChanPos].curMsgReadPos = allChans[i_id].totalMsgs;
    client.curChanPos++; // Increase client subscriptions count.

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
    CriticalChanAccess(i_id, 0);
    //allChans[i_id].msgs[allChans[i_id].totalMsgs].msg = calloc(MSG_MAX_LEN, sizeof(char));
    strcpy(allChans[i_id].msgs[allChans[i_id].totalMsgs].msg, msg);  // Add message to channel.

    // Set msg priority to YYYYMMDDHHmmSS to determine when it was posted.
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char* priorityCode = str_append(ToCharArray(tm.tm_year + 1900), 
        str_append(ToCharArray(tm.tm_mon), str_append(ToCharArray(tm.tm_mday), 
            str_append(str_append(ToCharArray(tm.tm_hour), ToCharArray(tm.tm_min)), ToCharArray(tm.tm_sec)))));
    long long tt = atoll(priorityCode); 
   
    // Assign read order priority to message if subbed to channel sent to.
    if(GetClientSubIndex(id) >= 0) {   
        allChans[i_id].msgs[allChans[i_id].totalMsgs].timeSent = tt;  
    } 

    allChans[i_id].totalMsgs++; // Increase total messages sent.
    sem_post(&chanMutex[i_id]);
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
    msg = "Channel Subscriptions\n"; 
    for(int i = 0; i < client.curChanPos; i++)
    {
        int i_id = atoi(client.channels[i].id); // Get target channel id from sub list.
        int c_index = GetClientSubIndex(client.channels[i].id); // Get index of channel in client subs.
        char* t_msg = "Channel: 111\tTotal Messages: 222\tRead: 333\tUnread 444\n"; 

        CriticalChanAccess(i_id, 1); // Check if can read.

        t_msg = replace_str(t_msg, "111", allChans[i_id].id); 
        t_msg = replace_str(t_msg, "222", ToCharArray(allChans[ i_id].totalMsgs));
        t_msg = replace_str(t_msg, "333", ToCharArray(client.channels[c_index].curMsgReadPos - client.channels[c_index].curSubPos));
        t_msg = replace_str(t_msg, "444", ToCharArray(allChans[i_id].totalMsgs - client.channels[c_index].curMsgReadPos));
        msg = str_append(msg, t_msg); // Append channel info to msg buffer.    
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
    CriticalChanAccess(atoi(id), 0); // Wait until can safely read.
    if(allChans[atoi(id)].totalMsgs - client.channels[GetClientSubIndex(id)].curMsgReadPos > 0) 
    {
        client.channels[GetClientSubIndex(id)].curMsgReadPos++; 
        sem_post(&chanMutex[atoi(id)]);
        char* m = allChans[atoi(id)].msgs[client.channels[GetClientSubIndex(id)].curMsgReadPos - 1].msg;
        
        return m;
    }
    sem_post(&chanMutex[atoi(id)]);
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
    long long nextMsgSentTime = 9999999999999;
   
    for(int i = 0; i < client.curChanPos; i++) // Loop through subscriptions.
    {
        if(client.channels[i].curMsgReadPos < allChans[atoi(client.channels[i].id)].totalMsgs)
        {
            int readPos = client.channels[i].curMsgReadPos;
            if(allChans[atoll(client.channels[i].id)].msgs[readPos].timeSent < nextMsgSentTime) // Check if msg higher priority.
            {             
                CriticalChanAccess(atoi(allChans[atoi(client.channels[i].id)].id), 0);                
                nextChanID = atoi(client.channels[i].id);
                nextMsgSentTime = allChans[atoll(client.channels[i].id)].msgs[readPos].timeSent;   
                sem_post(&chanMutex[atoi(allChans[atoi(client.channels[i].id)].id)]);
            }           
        } 
    }

    if(nextChanID >= 99999) // No new messages, return nothing.
        return "";
    
    client.channels[GetClientSubIndex(allChans[nextChanID].id)].curMsgReadPos++;
    return str_append(str_append(ToCharArray(nextChanID),":"), 
        allChans[nextChanID].msgs[client.channels[GetClientSubIndex(allChans[nextChanID].id)].curMsgReadPos-1].msg);
}





/*
    Writes unread msgs in real time for either a single or all chans.
    CONDITIONS:: Whether single chan or all and if exit.
    INPUT:: Client socket and buffer containing livestream cmd.
    OUTPUT:: Oldest unread message in real time of single or all chans.
*/
void LiveStream(int newsockfd, char buffer[BUFFER_SIZE])
{
    streaming = 1;
    char* newBuffer = calloc(BUFFER_SIZE, sizeof(char));
    
    if(strlen(buffer) >= 12) // If included an chan ID in cmd, construct as Next ID cmd.
        strcpy(newBuffer, replace_str(buffer, "LIVESTREAM", "NEXT"));
    
    while(streaming == 1) 
    {
        char* msg = calloc(BUFFER_SIZE, sizeof(char));

        if(strlen(newBuffer) > 0) { // Livestream msgs for specified channel.
            strcpy(msg, Next(newBuffer));
            if(strlen(msg) > 0)            
                WriteClient(newsockfd, msg); 
        }
        else { // Livestream msgs for all channels.            
            strcpy(msg, NextAll());
            if(strlen(msg) > 0)            
                WriteClient(newsockfd, msg); 
        }
        if(strlen(msg) <= 0) { // Livestream nothing if no msg.
            WriteClient(newsockfd, "​");
        }

        bzero(buffer , BUFFER_SIZE); 
        read(newsockfd , buffer , BUFFER_SIZE);

        if (strstr(buffer, "[+Hide-] EXEC EXIT SERVER") != NULL) { // Exit when recieve from client.
            bzero(buffer , BUFFER_SIZE); 
            streaming = 0;
            break;
        }
        sleep(.6);
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
   
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen; // Sock addr size.
    char bufferg[BUFFER_SIZE];
  

    bzero((char *) &serv_addr , sizeof(serv_addr)); // Clear any data in reference (make sure serv_addr empty).
    sockfd = socket(AF_INET, SOCK_STREAM, 0);       // IPv4, TCP, default to TCP.
    
    if(sockfd < 0) Error("Failed to open requested socket!\n");
    if(argc < 2) portno = DEFAULT_PORT; // Default port if not specified.
    else portno = atoi(argv[1]); // Custom port number.

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); 

    // If fail bind, try bind to next port.
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    {
        portno += 1;
        serv_addr.sin_port = htons(portno); 
        if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)         
        Error("Failed to bind socket address to memory!\n\nNOTE:: If Client(s) running, press Enter on their terminal Twice.\n\n");        
    }
        
    
    listen(sockfd , MAX_CLIENTS); // Listen on socket passively.
    clilen = sizeof(cli_addr);    


    // Shared memory (Client ID).
    if ((shmid[0] = shmget(IPC_PRIVATE, sizeof(int) * MAX_CLIENTS, IPC_CREAT | 0666)) < 0) { // Create mem segment.
        perror("shmget");
        exit(1);
    }
    shm_cliForks = shmat(shmid[0], NULL, 0); // Attach segment to data space.

    // Shared memory (Channels).
    if ((shmid[1] = shmget(IPC_PRIVATE, sizeof(struct Channel) * 256, IPC_CREAT | IPC_EXCL | 0666)) < 0) { // Create mem segment.
        perror("shmget");
        StopServer();
    }
    allChans = shmat(shmid[1], NULL, 0); // Attach segment to data space.     
    for(int i = 0; i < 256; i++) { // Populate channels with default values.
        //allChans[i].id = calloc(3, sizeof(char));
        strcpy(allChans[i].id, ToCharArray(i));
        allChans[i].curMsgReadPos = 0;
        allChans[i].totalMsgs = 0;
        allChans[i].curSubPos = 0;
    }   

    // Shared memory (Mutex id for each channel).
    if ((shmid[2] = shmget(IPC_PRIVATE, sizeof(sem_t) * 256, IPC_CREAT | 0666)) < 0) { // Create mem segment.
        perror("shmget");
        StopServer();
    }
    chanMutex = shmat(shmid[2], NULL, 0); // Attach segment to data space.
    for(int i = 0; i < 256; i++) { // Init all mutex locks.
        sem_init(&chanMutex[i], 1, 1);
    }

    // Multiprocessing.
    /////////////////////////////////////////////////
    pid_t pid;
    // Create client handler forks.
    for(int i=0;i<MAX_CLIENTS;i++) { 
        pid = fork();
        if(pid < 0)
            Error("ERROR, fork failed.");               
    }

    
    
    if(pid == 0)
    {  
        int id_index = *shm_cliForks;
        *shm_cliForks += 1;

        while(1){ // Keep reconnecting.
            newsockfd = accept(sockfd , (struct sockaddr *) &cli_addr , &clilen); // Accept client connection (waits until complete).
            if(newsockfd < 0) 
                Error("Client failed to connect to server. Something is wrong!\n"); 
            
            // Client setup & initialize first connection.
            client.curChanPos = 0;
            //client.id = calloc(3, sizeof(char));
            strcpy(client.id, ToCharArray(id_index)); 
            WriteClient(newsockfd, replace_str("Welcome! Your client ID is xxx.\n","xxx",client.id));
            
            while(servrun == 1) // Server running handling.
            {
                bzero(bufferg , BUFFER_SIZE); // Clear buffer.
                n = read(newsockfd , bufferg , BUFFER_SIZE); // Wait point, wait for client to write.            
                if(n < 0) // Read fail, try recoonect.
                    break;
                
                char* msg;
                
                
                if(strstr(bufferg , "UNSUB ") != NULL)
                    msg = Unsub(bufferg);          
                else if(strstr(bufferg , "SUB ") != NULL)                
                    msg = Sub(bufferg); 
                else if(strstr(bufferg , "CHANNELS") != NULL)
                    msg = Channels();
                else if(strstr(bufferg , "SEND ") != NULL)
                    msg = Send(bufferg);
                else if(strstr(bufferg , "BYE") != NULL)   
                {
                    Bye(newsockfd);
                }   
                else if(strstr(bufferg , "NEXT") != NULL)  
                    if(strlen(bufferg) >= 6)  
                        msg = Next(bufferg);
                    else
                        msg = NextAll();
                else
                    msg =  "";
                if(strstr(bufferg, "LIVESTREAM") != NULL)  
                    LiveStream(newsockfd, bufferg);

            

                int n = WriteClient(newsockfd, str_append("​", msg));   
                if(n < 0) // Write fail, try reconnect.
                    break;
            }
        }  
    }
    ////////////////////////////////////////////////////////

    for(int i=0;i<MAX_CLIENTS;i++) // Wait for all forks.
        wait(NULL); 

    // Close server and client sockets.
    close(sockfd);   
    close(newsockfd);

    // Detach from mem when all stop.
    shmdt(shm_cliForks); 
    shmdt(allChans);
    shmdt(chanMutex);

    // Remove from shared mem.
    shmctl(shmid[0], IPC_RMID, NULL); 
    shmctl(shmid[1], IPC_RMID, NULL); 
    shmctl(shmid[2], IPC_RMID, NULL); 
    return 0;
}