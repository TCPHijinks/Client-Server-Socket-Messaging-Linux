/*
    Required args: filename server_ipaddress portno
    e.g. ./client 127.0.0.1

    argv[0] filename
    argv[1] server_ipaddress
    argv[2] portno

*/
int sockfd;
int servrun = 1;
int streaming = 0;

#define BUFFER_SIZE 2040

#include <stdio.h>  // Standard input output - printf, scanf, etc.
#include <stdlib.h> // Contains var types, macros and some general functions (e.g atoi).
#include <string.h> // Contains string functionality (e.g string compare).
#include <strings.h>
#include <unistd.h> // Contains read, write and close functions.
#include <sys/types.h>  // Contain datatypes to do system calls (socket.h, in.h + more).
#include <sys/socket.h> // Definitions of structures for sockets (e.g. sockaddr struct).
#include <netinet/in.h> // Contain consts and structs needed for internet domain addresses.
#include <netdb.h>      // Defines hostent struct. Contains host info, including hostname and ip version for address (e.g. ipv4).
#include <signal.h>

 char buffer[BUFFER_SIZE]; // Store messages in heref ro data stream.

void error(const char *msg)
{
    perror(msg); // Output error number and message.
    exit(0);
}

/* Signal Handler for SIGINT */
void sigintHandler(int sig_num) 
{ 
    streaming = 0;
    signal(SIGINT, sigintHandler);    
    write(sockfd , "[+Hide-] EXEC EXIT SERVER" , strlen("[+Hide-] EXEC EXIT SERVER"));
     // Wait to read buffer from server.
    bzero(buffer , BUFFER_SIZE); 
    int n = read(sockfd , buffer , BUFFER_SIZE); 
    if(n < 0) error("Can't read. Something broke!\n");
    printf("%s",buffer);
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


int main(int argc , char *argv[])
{
    signal(SIGINT, sigintHandler); 

    int portno, n;
    struct sockaddr_in serv_addr; // Socket address.
    struct hostent *server;
    
   
    if(argc < 3){ // Error if not given all args (hostname and port no).
        fprintf(stderr, "usage %s hostname port.\n", argv[0]); // Print to 'Standard Error' Stream output.
        exit(1);
    }
    
    portno = atoi(argv[2]); // Get port no from command line args.
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // File descriptor has: IPv4, TCP, default to TCP.
    if(sockfd < 0) // Failure (-1).   
        error("Error opening socket.");

    server = gethostbyname(argv[1]); // Get host using given ip address.
    if(server == NULL)
    {
        fprintf(stderr , "Error, no such host. Either server offline or invalid ip address.");

    }

    bzero((char *) &serv_addr , sizeof(serv_addr)); // Ensure serv addresss empty.
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr_list[0] , (char *) &serv_addr.sin_addr.s_addr, server->h_length); // Copy n bytes from hostnet to serv_addr.
    serv_addr.sin_port = htons(portno); // Set port to 'Host to network shot'.
    // If fail connect, try connect to next port.
    if(connect(sockfd , (struct sockaddr *) &serv_addr , sizeof(serv_addr)) < 0)
    {
        portno += 1;
        serv_addr.sin_port = htons(portno);
        if(connect(sockfd , (struct sockaddr *) &serv_addr , sizeof(serv_addr)) < 0)
            error("Connection Failed.");
    }
        
        // Wait to read buffer from server.
        bzero(buffer , BUFFER_SIZE); 
        n = read(sockfd , buffer , BUFFER_SIZE); 
        if(n < 0) error("Can't read. Something broke!\n");
        printf("%s",buffer);

    while(servrun == 1)
    {    
        bzero(buffer , BUFFER_SIZE);
        fgets(buffer , BUFFER_SIZE , stdin); 
        strcpy(buffer, replace_str(buffer, "[+Hide-] EXEC EXIT SERVER", "N/A")); // Prevent client closing server. 
        n = write(sockfd , buffer , strlen(buffer));

        if(n < 0) break;    
        if(strstr(buffer , "BYE") != NULL) break;
        if(strstr(buffer , "LIVESTREAM") != NULL) streaming = 1;

        while (streaming == 1)
        {          
            bzero(buffer , BUFFER_SIZE); // Clear buffer.
            read(sockfd , buffer , BUFFER_SIZE);
            
            printf("%s",buffer);
            if(streaming == 0)
                break;

            char* msg = "​​";
            write(sockfd , msg , strlen(msg));
            sleep(.6);
        }

        // Wait to read buffer from server.
        bzero(buffer , BUFFER_SIZE); 
        n = read(sockfd , buffer , BUFFER_SIZE); 
        if(n < 0) break;
        printf("%s",buffer);
       
    }    
    close(sockfd);
    return 0;
}