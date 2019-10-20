#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>  // Standard input output - printf, scanf, etc.
#include <stdlib.h> // Contains var types, macros and some general functions (e.g atoi).
#include <string.h> // Contains string functionality, specifically string comparing.
#include <unistd.h> // Contains read, write and close functions.
#include <sys/types.h>  // Contain datatypes to do system calls (socket.h, in.h + more).
#include <sys/socket.h> // Definitions of structures for sockets (e.g. sockaddr struct).
#include <netinet/in.h> // Contain consts and structs needed for internet domain addresses.
#include <signal.h>


const int MAX = 4;
#define SHMSZ     27

int shmid;
key_t cliForks_key;
//char *shm;
int *shm_cliForks = 0;

#define BUFFER_SIZE 2040
#define DEFAULT_PORT 12345
const int MAX_ID_VAL = 255;
const int MIN_ID_VAL = 0;
int servrun = 1;        // Server running state.
int sockfd , newsockfd; // Socket file discriptors.
int portno, n;
char buffer[BUFFER_SIZE];
struct sockaddr_in serv_addr, cli_addr;
socklen_t clilen; // Sock addr size.

void StopServer()
{
    close(newsockfd);
    close(sockfd);
    exit(1);
}

// Outputs error to terminal and exits program.
void Error(const char *msg)
{
    perror(msg); 
    StopServer();
}


void ClientConnectionHandle()
{
    if(*shm_cliForks > MAX)
        exit(-1); // Too many client forks.
    cliForks_key = 5678; // Get shared memory segment via its name.

    if ((shmid = shmget(cliForks_key, SHMSZ, 0666)) < 0) { // Locate segment in mem.
      perror("shmget");
      exit(1);
    }
    shm_cliForks = (int*) shmat(shmid, NULL, 0); // Assign to shared memory.  

    newsockfd = accept(sockfd , (struct sockaddr *) &cli_addr , &clilen); // Accept client connection (waits until complete).
    if(newsockfd < 0) 
        Error("Client failed to connect to server. Something is wrong!\n"); 

    *shm_cliForks += 1;
    printf("%d [son] has incremented shared mem to [%d].\n",getpid(),*shm_cliForks);
}



void ServerSetup(int argc, char * argv[])
{
   
    
    bzero((char *) &serv_addr , sizeof(serv_addr)); // Clear any data in reference (make sure serv_addr empty).
    sockfd = socket(AF_INET, SOCK_STREAM, 0);       // IPv4, TCP, default to TCP.
    
    if(sockfd < 0) Error("Failed to open requested socket!\n");
    if(argc < 2) portno = DEFAULT_PORT; // Default port if not specified.
    else portno = atoi(argv[1]); // Custom port number.

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); 

  
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        Error("Failed to bind socket address to memory!\n");
    
    listen(sockfd , MAX); // Listen on socket passively.
    clilen = sizeof(cli_addr); 
}


int main(int argc, char * argv[]) 
{      
    ServerSetup(argc, argv);
    cliForks_key = 5678; // Name shared memory.

    if ((shmid = shmget(cliForks_key, SHMSZ, IPC_CREAT | 0666)) < 0) { // Create mem segment.
        perror("shmget");
        exit(1);
    }

    shm_cliForks = (int*) shmat(shmid, NULL, 0); // Attach segment to data space.
      



    for(int i=0;i<6;i++) // loop will run n times (n=5) 
    { 
        pid_t pid = fork();
        if(pid < 0){
            printf("ERROR, fork failed.");
        }
        else if(pid == 0){
            printf("NUM CLI:%d.\n",*shm_cliForks);
            
            printf("%d::[son] pid %d from [parent] pid %d\n",i,getpid(),getppid()); 
        }            
    }

    ClientConnectionHandle();
    exit(99); // Can pass any return val, 0 is default for work but doesnt matter

        // Get child exit code and retrieve Exit Status from returned val.
        // TODO: Use exit val to determine if child was handling a client connection or not. 
        //  If handling, decrement number off client handlers so can fork() new handler child.
        int exit_status = 0;
        pid_t childpid = wait(&exit_status); 
        int childReturnVal = WEXITSTATUS(exit_status); 

        if(childReturnVal > -1) // If child was handling client, make new ffork available.
            *shm_cliForks -= 1;


        printf("Parent of %d knows that child of %d has exited with value of %d.\n", 
            (int) getpid(), (int) childpid, childReturnVal);
    
    for(int i=0;i<6;i++) // loop will run n times (n=5) 
        wait(NULL); 
    
    // Detach from mem when all stop.
    shmdt(shm_cliForks); // Detach shared mem segment.
    shmctl(shmid, IPC_RMID, NULL); // Remove from shared mem.
} 