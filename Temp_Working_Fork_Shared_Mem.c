/*
    Working basic fork with shared memory.
*/
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHMSZ     27




void ClientConnectionHandle()
{
  int shmid;
  key_t key;
  char *shm, *s;


  key = 5678; // Get shared memory segment via its name.

  if ((shmid = shmget(key, SHMSZ, 0666)) < 0) { // Locate segment in mem.
      perror("shmget");
      exit(1);
  }

  if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) { // Attach to local data space.
      perror("shmat");
      exit(1);
  }


  for (s = shm; *s != 0; s++) // Read what parent put into mem.
      putchar(*s);
  putchar('\n');

  *shm = '*'; // Change first char of segment to '*' to show that read segment.



}



int main(int argc, char *argv[]){
  
  char c;
  int shmid;
  key_t key;
  char *shm, *s;

  key = 5678; // Name shared memory.

  if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) { // Create mem segment.
      perror("shmget");
      exit(1);
  }

  if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) { // Attach segment to data space.
      perror("shmat");
      exit(1);
  }

  // Add starting data.
  s = shm;
  for (c = 'a'; c <= 'z'; c++)
      *s++ = c;
  *s = 0;

  pid_t pid = fork();
  if(pid < 0){
    printf("ERROR, fork failed.");
  }
  else if(pid == 0){
    printf("I am the child with pid %d\n", (int)getpid());
    ClientConnectionHandle();
    exit(99); // Can pass any return val, 0 is default for work but doesnt matter
  }
  else if(pid > 0){
    printf("I am the parent with pid %d\n", (int) getpid());
    printf("I forked a child of pid %d\n", (int) pid);
    while (*shm != '*')
        sleep(1);
    printf("Done waiting on child to edit shared mem.");
  }

  // Only parent reach here because of child exit.
  int status = 0;
  pid_t childpid = wait(&status); // Wait for child to end.
  int childReturnVal = WEXITSTATUS(status); // Get only the return value of child (otherwise has more encoded data).
  printf("Parent of %d knows that child of %d has exited with value of %d.\n", (int) getpid(), (int) childpid, childReturnVal);
  
  
  return 0;
}