#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX 5
#define SHMSZ     27

int shmid;
key_t cliForks_key;
//char *shm;
int *shm_cliForks = 0;



void ClientConnectionHandle()
{
    cliForks_key = 5678; // Get shared memory segment via its name.

    if ((shmid = shmget(cliForks_key, SHMSZ, 0666)) < 0) { // Locate segment in mem.
      perror("shmget");
      exit(1);
    }
    shm_cliForks = (int*) shmat(shmid, NULL, 0); // Assign to shared memory.  
    *shm_cliForks += 2;
    printf("%d [son] has incremented shared mem to [%d].\n",getpid(),*shm_cliForks);
}






int main() 
{      

    cliForks_key = 5678; // Name shared memory.

    if ((shmid = shmget(cliForks_key, SHMSZ, IPC_CREAT | 0666)) < 0) { // Create mem segment.
        perror("shmget");
        exit(1);
    }

    shm_cliForks = (int*) shmat(shmid, NULL, 0); // Attach segment to data space.
      



    for(int i=0;i<2;i++) // loop will run n times (n=5) 
    { 
        pid_t pid = fork();
        if(pid < 0){
            printf("ERROR, fork failed.");
        }
        else if(pid == 0){
            printf("%d::[son] pid %d from [parent] pid %d\n",i,getpid(),getppid()); 
            ClientConnectionHandle();
            exit(99); // Can pass any return val, 0 is default for work but doesnt matter
        }

        // Get child exit code and retrieve Exit Status from returned val.
        // TODO: Use exit val to determine if child was handling a client connection or not. 
        //  If handling, decrement number off client handlers so can fork() new handler child.
        int exit_status = 0;
        pid_t childpid = wait(&exit_status); 
        int childReturnVal = WEXITSTATUS(exit_status); 
        printf("Parent of %d knows that child of %d has exited with value of %d.\n", 
            (int) getpid(), (int) childpid, childReturnVal);
    } 
    for(int i=0;i<2;i++) // loop will run n times (n=5) 
        wait(NULL); 
    
    shmdt(shm_cliForks); // Detach shared mem segment.
    shmctl(shmid, IPC_RMID, NULL); // Remove from shared mem.
} 