// bin_adder.c was created by Mark Renard on 3/13/2020
//
// This file contains a program which adds a number of integers in a shared
// memory array and stores the result in its assigned index

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>

#include "perrorExit.h"
#include "sharedMemory.h"
#include "shmkey.h"
#include "semname.h"

/* Prototypes */
static void launchChildren(char * argv[], int size);
static pid_t launchChild(char * argv[], int index, int size);
static void updateLogFile(pid_t pid, int index, int size);

/* Named Constants */
static const int BUFF_SZ = 100;		// Number of bytes in char buffers
static const int MAX_RUNNING = 18;	// Max number of simultaneous children
static const char * CHILD_PATH = "./bin_adder";	// Path to executable

/* Static Global Variables */
static char * shm = NULL;       // Pointer to the shared memory region
FILE * logFile = NULL;          // Pointer to log file in shared memory
static sem_t * sem = NULL;      // Semaphore protecting logFile

int main(int argc, char * argv[]){
	int * intArray;			// Pointer to the shared int array

	int pid = getpid();		// Pid of the current process
	int index = atoi(argv[1]);	// Index of the current process
	int size = atoi(argv[2]);	// Number of ints process should add
	int shmSize = atoi(argv[3]);	// Size of the shared memory region

	// Simply prints info on the child
	printf("bin_adder %d: index %d, size %d\n", pid, index, size);
	fflush(stdout);

	// Gets pointers to shared memory items
	shm = sharedMemory(shmSize, 0);
	logFile = (FILE *) shm;
	intArray = (int *)(shm + sizeof(FILE *));

	// Opens existing semaphore
	if ((sem = sem_open(SEMNAME, 0)) == SEM_FAILED)
		perrorExit("Failed opening existing semaphore");

	// Launches children if index is 0 and number of ints is greater than 2
	if (index == 0 && size > 2) launchChildren(argv, size);

	// Performs addition
	intArray[index] = intArray[index] + intArray[index + 1];
	
	// Writes to log at most 5 times
	updateLogFile(pid, index, size);
	
	return 0;

}

// Launches child bin_adders that add other portions of the shared array
static void launchChildren(char * argv[], int size){
	int index = 2;		  // Stores index of each child process
	int running = 0;	  // The number of children currently executing
	int completed = 0;	  // The number of children that finished
	int total = (size - 1)/2; // Number of children required for computation
	pid_t pid = 0;		  // Temp storage for child pids

	while (running + completed < total){

		// Creates bin_adder child with new index and size
		launchChild(argv, index, 2);

		// Updates index and number of running children	
		index += 2;
		running++;

		// Waits if maximum simultaneous processes reached
		if (running == MAX_RUNNING){
			printf("\n");
			fflush(stdout);

			sleep(1);
			
			// Waits for child to finish
			while ((pid = wait(NULL)) == -1 && errno == EINTR);

			// Updates number of running and completed children
			running--;
			completed++;
		}
	}

	// Waits for running processes to finish
	while (running > 0){
		while ((pid = wait(NULL)) == -1 && errno == EINTR);
		running--;
	}

}

// Launches a single child bin_adder
static pid_t launchChild(char * argv[], int index, int size){
	pid_t pid;			// Returned pid
	char sizeBuff[BUFF_SZ];		// Char buff for size
	char indexBuff[BUFF_SZ];	// Char buff for index

	// Sets index argument for new child
	sprintf(indexBuff, "%d", index);
	argv[1] = indexBuff;

	// Sets size argument for new child
	sprintf(sizeBuff, "%d", size);
	argv[2] = sizeBuff;

	if ((pid = fork()) == -1) perrorExit("Failed to fork");

	if (pid == 0){
		execv(CHILD_PATH, argv);
		perrorExit("Faild to exec child");
	}

	return pid;
}

// Updates the log file with pid, index, and the number of integers added
static void updateLogFile(pid_t pid, int index, int size){
	time_t current_time;
	
	// Seeds random number generator
	srand((unsigned int) time(NULL));	
	
	// Follows the template provided in ass3.pdf
	int i;
	for (i = 0; i < 5; i++){

		// Sleeps for random ammount of time (between 0 and 3 seconds)	
		sleep(random() % 4);

		current_time = time(NULL);
		fprintf(stderr, 
			"%s - Process %d attempting to enter critical section", 
			ctime(&current_time),
			index
		);

		// Waits for semaphore
		sem_wait(sem);

		/* Critical section */
		fprintf(stderr, "Process %d in critical section", index);
		sleep(1);
		fprintf(logFile, "%d  %d  %d\n",(int)pid, index, size);
		sleep(1);

		// Signals semaphore
		sem_post(sem);
	}
}

