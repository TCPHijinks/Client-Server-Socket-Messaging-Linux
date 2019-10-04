#include <stdio.h>  // Standard input output - printf, scanf, etc.
#include <stdlib.h> // Contains var types, macros and some general functions (e.g atoi).
#include <string.h>
#include <unistd.h>
#include <sys/types.h>  // Contain datatypes to do system calls (socket.h, in.h + more).
#include <sys/socket.h> // Definitions of structures for sockets (e.g. sockaddr struct).
#include <netinet/in.h> // Contain consts and structs needed for internet domain addresses.


void error(const char *msg)
{
    perror(msg); // Output error number and message.
    exit(1);
}


int main(int argc, char * argv[])
{
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

        while(1)
        {
            bzero(buffer , 255); // Clear buffer.
            n = read(newsockfd , buffer , 255); // New file discriptor on connected client, buffer and its size.
            if(n < 0)
                error("Error on read.");
            printf("Client : %s\n" , buffer); // Print buffer message.
            bzero(buffer, 255);
            fgets(buffer , 255, stdin); // Read buytes from stream.

            n = write(newsockfd , buffer, strlen(buffer)); // Write to client socket, send buffer, set string length of buffer.
            if(n < 0)
                error("Error on write.");

            int i = strncmp("Bye" , buffer , 3); // String compare - check if exit. Check for 'bye' in buffer of length 3.
            if(i == 0)
                break;
        }
        close(newsockfd);
        close(sockfd);
        
        return 0;
}