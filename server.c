/* 
    TO DO:
    > Replace read() with select (in client & serv) so loop doesn't get stuck passively waiting for client input.
    > Don't accept ctrl+c from client as will shut down serv as well (maybe do as a client-side fix?)
        * Allow client to accept ctrl+c as this should shut it down.
    > Formatting of replace_str when invalid format e.g. "fetfaw".
        * Allocate needed memory so replacement can be bigger.
*/

/*
    Required args: filename portno
    e.g. ./server 9999

    argv[0] filename
    argv[1] portno

*/

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




char *replace_str(char *str, char *orig, char *rep)
{
  static char buffer[4096];
  char *p;

  if(!(p = strstr(str, orig)))  // Is 'orig' even in 'str'?
    return str;

  strncpy(buffer, str, p-str); // Copy characters from 'str' start to 'orig' st$
  buffer[p-str] = '\0';

  sprintf(buffer+(p-str), "%s%s", rep, p+strlen(orig));

  return buffer;
}



char id[4]; // Id length including end \0.
int subscribe(char buffer[255])
{       
    memcpy(id , &buffer[5] , 4); // Get 3 char long id inside of <>.
    id[3] = '\0'; // Set string end.

    int i; // ID as an integer. 
    sscanf(id, "%d", &i); // Try convert id string to int.

    char temp[4];
    sprintf(temp, "%d" , i);
    if(strcmp(temp, id) != 0) // Invalid channel id return error.
        return -1;
        //printf("Invalid ID of %d != %s" , i , id);
    //else 
      //  printf("Valid ID of %d == %s", i , id);
    return 0;
}




static volatile sig_atomic_t servrun = 1;
static void sig_handler(int _)
{
    (void)_;
    servrun = 0;
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
    char buffer[255]; // Store messages in heref ro data stream.

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
    
    listen(sockfd , 5); // Listen on socket using 'socket file discriptor', allow maximum of 'n' (5) connections to server at a time.
    clilen = sizeof(cli_addr); // Set client address memory size.

    newsockfd = accept(sockfd , (struct sockaddr *) &cli_addr , &clilen);

    if(newsockfd < 0)
        error("Error on Accept,\n");
    
    

    while(servrun)
    {
        bzero(buffer , 255); // Clear buffer.

        // Waits here until response from client.
        n = read(newsockfd , buffer , 255); // New file discriptor on connected client, buffer and its size.
        if(n < 0)
            error("Error on read.");
        printf("Client : %s" , buffer); // Print buffer message.
        
        char* msg;
        if(strstr(buffer , "SUB") != NULL)
        {          
            // Set relevant msg with channel id.      
            if(subscribe(buffer) == 0) 
                strcpy(msg , replace_str("Subscribed to channel <xxx>.\n","xxx",id));  
            else
                strcpy(msg , replace_str("Invalid channel: <xxx>.\n","xxx",id));  
        }
        else
        {
            strcpy(msg , "INVALID INPUT.\n");
        }
        
        // Write to client.
        n = write(newsockfd , msg, strlen(msg));
        if(n < 0)
            error("Error on write.");       
    }
        
    close(newsockfd);
    close(sockfd);
    
    return 0;
}