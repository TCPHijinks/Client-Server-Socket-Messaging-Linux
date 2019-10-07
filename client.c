/*
    Required args: filename server_ipaddress portno
    e.g. ./client 127.0.0.1

    argv[0] filename
    argv[1] server_ipaddress
    argv[2] portno

 */

#include <stdio.h>  // Standard input output - printf, scanf, etc.
#include <stdlib.h> // Contains var types, macros and some general functions (e.g atoi).
#include <string.h> // Contains string functionality (e.g string compare).
#include <unistd.h> // Contains read, write and close functions.
#include <sys/types.h>  // Contain datatypes to do system calls (socket.h, in.h + more).
#include <sys/socket.h> // Definitions of structures for sockets (e.g. sockaddr struct).
#include <netinet/in.h> // Contain consts and structs needed for internet domain addresses.
#include <netdb.h>      // Defines hostent struct. Contains host info, including hostname and ip version for address (e.g. ipv4).
#include <signal.h>

void error(const char *msg)
{
    perror(msg); // Output error number and message.
    exit(0);
}

static volatile sig_atomic_t servrun = 1;
static void sig_handler(int _)
{
    (void)_;
    servrun = 0;
}


int main(int argc , char *argv[])
{
    signal(SIGINT, sig_handler);

    int sockfd , portno, n;
    struct sockaddr_in serv_addr; // Socket address.
    struct hostent *server;
    
    char buffer[255]; // Store messages in heref ro data stream.
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
    bcopy((char *) server->h_addr , (char *) &serv_addr.sin_addr.s_addr, server->h_length); // Copy n bytes from hostnet to serv_addr.
    serv_addr.sin_port = htons(portno); // Set port to 'Host to network shot'.
    if(connect(sockfd , (struct sockaddr *) &serv_addr , sizeof(serv_addr)) < 0)
        error("Connection Failed.");



    while(servrun)
    {
        bzero(buffer , 255); // Empty.
        n = read(sockfd , buffer , 255); // Read buffer from server.
        if(n < 0)
            error("Error on read.");
        printf("%s",buffer);

        bzero(buffer , 255);
        fgets(buffer , 255 , stdin); // Pass buffer to server using standard input stream. (wait point)              
        n = write(sockfd , buffer , strlen(buffer)); // Write to server.
        if(n < 0)
            error("Error on write.");

       // int i = strncmp("SERV_CLOSE" , buffer , 10); // String compare - check if exit. Check for 'bye' in buffer of length 3.
       // if(i == 0)
       //     break;
    }
    
    close(sockfd);
    return 0;

}