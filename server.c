 // aPPEND TO MSG BUFFER.
            //strcpy(msg, str_append(msg, "HAHAHAHAHAHA"));//////////////////////
            //strcpy(msg, str_append(msg, "__HAHAHAHAHAHA"));///////////////////



/* 
    TO DO:
    > Setup replacement for CHANNELS command.
    > Replace read() with select (in client & serv) so loop doesn't get stuck passively waiting for client input.
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


#include <stdio.h>  // Standard input output - printf, scanf, etc.
#include <stdlib.h> // Contains var types, macros and some general functions (e.g atoi).
#include <string.h> // Contains string functionality, specifically string comparing.
#include <unistd.h> // Contains read, write and close functions.
#include <sys/types.h>  // Contain datatypes to do system calls (socket.h, in.h + more).
#include <sys/socket.h> // Definitions of structures for sockets (e.g. sockaddr struct).
#include <netinet/in.h> // Contain consts and structs needed for internet domain addresses.
#include <signal.h>

void error(const char *msg)
{
    perror(msg); // Output error number and message.
    exit(1);
}

struct Message
{
    char* chnid;
    struct Message* next;
};


struct Channel
{
    char id[3];
    int curMsgReadPos;
    int totalMsgs;
    struct Message msgs[100];
};

void chan_addMsg(struct Message msg)
{
    // Have buffer overflow in case of too many messages.
}



struct Client
{
    char id[3];
    int curChanPos;
    struct Channel channels[255];
} client;



char * toCharArray(int number)
{
    int n = 1; // Num of digits in integer.
    if(number > 9) { n = 2; }
    if(number > 99) { n = 3; }
    
    char *cArray = calloc(n, sizeof(char));
    sprintf(cArray, "%d" , number);
    return cArray;
}





char *replace_str(char *str, char *old, char *new)
{
  static char buff[4096];
  char *p;

  if(!(p = strstr(str, old)))  // Is 'orig' even in 'str'?
    return str;
   
  strncpy(buff, str, p-str); // Copy characters from 'str' start to 'orig' st$
  buff[p-str] = '\0';
  sprintf(buff+(p-str), "%s%s", new, p+strlen(old));
  return buff;
}



int ValidID(char* id) 
{     
    char *t_id = id;
    if(strlen(t_id) < 3)
        return -1;  
    while(*t_id != '\0')
    {        
        if(*t_id < '0' || *t_id> '9')
            return -1;
        t_id++;    
    }

    // Convert id to integer.
    int i_id = 0; 
    sscanf(id, "%d", &i_id); 

    printf("ID VAL is:%d",i_id);
    if(i_id > MAX_ID_VAL || i_id < MIN_ID_VAL) // Fail if id outside allowed range.
        return -1;   
    
    return 0;
}



int cli_alreadySubbed(char* chanId) // TO DO: Optimize algorithm later.
{    
    for(int i = 0; i < client.curChanPos; i++)
    {        
        if(strcmp(chanId, client.channels[i].id) == 0) 
        {    
            return 0; // Already subbed to.
        }             
    }  
    return -1; // No match.
}



void AddNewChan(char* idchan)
{
    strcpy(client.channels[client.curChanPos].id, idchan);
    client.channels[client.curChanPos].curMsgReadPos = 0;  
    client.channels[client.curChanPos].totalMsgs = 0;
    client.curChanPos++;    
    return;
}


int GetChanIndex(char* id)
{
    int n = client.curChanPos;
    for(int i = 0; i < n; i++)
    {
        if(strcmp(client.channels[i].id, id) == 0)
            return i;
    }
    return -1; // Failed to find.
}


char* Unsub(char buffer[BUFFER_SIZE])
{
    char id[4];
    memcpy(id , &buffer[7] , 4); // Get 3 char long id inside of <> + /0 end.
    id[3] = '\0'; 
   
    if(ValidID(id) < 0) // Invalid id.
        return replace_str("Invalid channel: <xxx>.\n","xxx",id);
    
    int i = GetChanIndex(id);
    if(i < 0) // Not subscribed to this chan id.
        return replace_str("Not subscribed to channel: <xxx>.\n","xxx",id);
    
    
   
    for(;i < client.curChanPos; i++)
    {
        strcpy(client.channels[i].id , client.channels[i+1].id);
        client.channels[i].curMsgReadPos = client.channels[i+1].curMsgReadPos;
        client.channels[i].totalMsgs = client.channels[i+1].totalMsgs;         
        memcpy(client.channels[i].msgs, client.channels[i+1].msgs, sizeof(client.channels[i].msgs));   
        printf("\nwork\n");     
    }
    if(client.curChanPos > 0)
        client.curChanPos -= 1;
    
    return replace_str("Usubscribed from channel: <xxx>.\n","xxx",id);
}



char* Sub(char buffer[BUFFER_SIZE])
{       
    char id[4];
    memcpy(id , &buffer[5] , 4); // Get 3 char long id inside of <>.
    id[3] = '\0'; // Set string end.

    if(ValidID(id) < 0) // Return if invalid id.
        return replace_str("Invalid channel: <xxx>.\n","xxx",id);

    if(cli_alreadySubbed(id) == 0)
        return replace_str("Already subscribed to channel <xxx>.\n","xxx",id);
    
    AddNewChan(id);
    return replace_str("Subscribed to channel <xxx>.\n","xxx",id);
}



// Handler for program exit via ctrl+c in terminal.
static volatile sig_atomic_t servrun = 1;
static void sig_handler(int _)
{
    (void)_;
    servrun = 0;
}



// Append strings and return result.
char* str_append(char* str1, char* str2)
{
    int n = strlen(str1) + strlen(str2);
    char* t_msg = calloc(n, sizeof(char));
    strcat(t_msg , str1);
    strcat(t_msg, str2);
    
    return t_msg;
}



// Write to client.
void writecli(int newsockfd, char* buffer)
{    
    int n = write(newsockfd , buffer, strlen(buffer));
    if(n < 0)
        error("Error on write."); 
}







// Return list of subscribed channels.
char* get_cli_channels_msg()
{
    printf("CUR POS is %d",client.curChanPos);
    if(client.curChanPos <= 0)
        return " ";

    char *msg = calloc(BUFFER_SIZE , sizeof(char)); 
    strcpy(msg, "Channel Subscriptions\n"); 
    for(int i = 0; i < client.curChanPos; i++)
    {
        char* t_msg = "Channel: <111>\tTotal Messages: 222\tRead: 333\tUnread 444\n";        
        t_msg = replace_str(t_msg, "111", client.channels[i].id);
        t_msg = replace_str(t_msg, "222", toCharArray(client.channels[i].totalMsgs));
        t_msg = replace_str(t_msg, "333", toCharArray(client.channels[i].curMsgReadPos));
        t_msg = replace_str(t_msg, "444", toCharArray(client.channels[i].totalMsgs - client.channels[i].curMsgReadPos));
        strcpy(msg, str_append(msg, t_msg));
     
    }
    return msg;
}



int main(int argc, char * argv[])
{
    signal(SIGINT, sig_handler);
   
    if(argc < 2)
    {
        fprintf(stderr , "Port Num not provided. Program terminated.\n");
        exit(1);
    }

    int sockfd , newsockfd, portno, n;
    char buffer[BUFFER_SIZE]; // Store messages in heref ro data stream.

    struct sockaddr_in serv_addr, cli_addr;// Socket address.
    socklen_t clilen; // Size of socket address in bytes.
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // IPv4, TCP, default to TCP.
    if(sockfd < 0) // Failure.
    {
        error("Error opening socket.");
    }

    bzero((char *) &serv_addr , sizeof(serv_addr)); // Clear any data in reference (make sure serv_addr empty).
    portno = atoi(argv[1]); // Convert string to integar.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); // Host to newtowrk shot.


    // Assign socket address to memory.
    if(bind(sockfd , (struct sockaddr *) &serv_addr , sizeof(serv_addr)) < 0) //  Returns -1 on failure.    
        error("Binding failed.");
    
    listen(sockfd , MAX_CLIENTS); // Listen on socket using 'socket file discriptor', allow maximum of 'n' (5) connections to server at a time.
    clilen = sizeof(cli_addr); // Set client address memory size.

    newsockfd = accept(sockfd , (struct sockaddr *) &cli_addr , &clilen);
    if(newsockfd < 0)
        error("Error on Accept,\n");
  

    // Client setup.
    //client.id = calloc(3, sizeof(char));
    client.curChanPos = 0;
    strcpy(client.id, "001") ; //////
    
    // Starting welcome message.
    writecli(newsockfd, replace_str("Welcome! Your client ID is <xxx>.\n","xxx",client.id));


    while(servrun)
    {
        bzero(buffer , BUFFER_SIZE); // Clear buffer.
        n = read(newsockfd , buffer , BUFFER_SIZE); // Wait point, wait for client to write.
        if(n < 0)
            error("Error on read.");            
        printf("Client : %s" , buffer); // Print buffer message.
        

        char* msg;
        if(strstr(buffer , "UNSUB") != NULL)
            strcpy(msg , Unsub(buffer));          
        else if(strstr(buffer , "SUB") != NULL)                
            strcpy(msg , Sub(buffer)); 
        else if(strstr(buffer , "CHANNELS") != NULL)
            strcpy(msg, get_cli_channels_msg());
        else
        {
            strcpy(msg , "INVALID INPUT.\n");
        }
        
        // Write to client.
        writecli(newsockfd, msg);   
    }
        
    close(newsockfd);
    close(sockfd);
    
    return 0;
}



