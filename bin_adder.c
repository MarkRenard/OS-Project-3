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
#include <pthread.h>
#include <math.h>

#include "perrorExit.h"
#include "sharedMemory.h"
#include "shmkey.h"
#include "constants.h"

/* macro for determining the max of two quantities */
#define max(x,y) ((x >= y) ? x : y)

static void printArray(int * array, int size){
        int i;
        for (i = 0; i < size; i++){
                printf("%d, ", array[i]);
        }
        printf("\n");
}

/* Prototypes */
static void launchChildrenM1(char * argv[], int size);
static void launchChildrenM2(char * argv[], int size, int totalChildren, int gap);
static pid_t launchChild(char * argv[], int index, int size);
static void updateLogFile(pid_t pid, int index, int size);
static void criticalSection(pid_t pid, int index, int size);
static void sumInts(int * intArray, int resultIndex, int numInts);
static void leftShiftInts(int * intArray, int numInts, int gap);

/* Static Global Variables */
static char * shm = NULL;       	// Pointer to the shared memory region
FILE * logFile = NULL; 		        // Pointer to log file in shared memory
static pthread_mutex_t * sem = NULL;    // Semaehore protecting logFile

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
	sem = (pthread_mutex_t *)(shm);
	intArray = (int *)(shm + sizeof(pthread_mutex_t));

	// Launches children if called with -1 or -2 as an index by master
	if (index < 0){
		int totalChildren;
		int gap;

		// Launches children in mode 1
		if (index == -1) {
			printf("Method 1 procedure on %d ints\n", size);

			totalChildren = (int)ceil(size/2.0);
			gap = 2;
			launchChildrenM2(argv, size, totalChildren, gap);
			leftShiftInts(intArray, size, 2);

		// Launches children in mode 2
		} else if (index == -2) {
			printf("Method 2 procedure on %d ints\n", size);
			
			int totalChildren = \
				(int)ceil(size/log((double)size)/log(2.0));
			int gap = (int)ceil(log((double)size)/log(2.0));
 
			launchChildrenM2(argv, size, totalChildren, gap);
			leftShiftInts(intArray, size, gap);
			
		}

	// Performs computation if this process is a child of a bin_adder		
	} else {
		sumInts(intArray, index, size);
	}
	
	// Writes to log, accessing critical section at most 5 times
	updateLogFile(pid, index, size);
	
	return 0;

}

// Launches child bin_adders that add pairs of integers
static void launchChildrenM1(char * argv[], int size){
	int index = 0;		  // Stores index of each child process
	int running = 0;	  // The number of children currently executing
	int completed = 0;	  // The number of children that finished
	int total = (int)ceil(size/2.0); // Number of children required for computation
	pid_t pid = 0;		  // Temp storage for child pids

	while (running + completed < total){

		// Creates bin_adder child with new index and size
		launchChild(argv, index, 2);

		// Updates index and number of running children	
		index += 2;
		running++;

		// Waits if maximum simultaneous processes reached
		if (running == MAX_RUNNING){
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

// Launches children using method 2
static void launchChildrenM2(char * argv[], int size, int totalChildren, int gap){
	int index = 0;		 // Stores index of each child process
	int running = 0;	 // The number of children currently executing
	int completed = 0;	 // The number of children that finished
	pid_t pid = 0;		 // Temp storage for child pids

	while (running + completed < totalChildren){

		// Checks for gap change on last iteration
		if (index == totalChildren - 1)
			gap = max(size - totalChildren * gap, 2);

		// Creates bin_adder child with new index and size
		launchChild(argv, index, gap);

		// Updates index and number of running children	
		index += gap;
		running++;

		// Waits if maximum simultaneous processes reached
		if (running == MAX_RUNNING){
			
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
		sleep(random() % (MAX_SLEEP - MIN_SLEEP + 1) + MIN_SLEEP);

		current_time = time(NULL);
		fprintf(stderr, 
			"Process %d attempting to enter critical section - %s",
			(int)pid,
			ctime(&current_time)
		);

		// Waits for semaphore
		pthread_mutex_lock(sem);

		/* Critical section */
		criticalSection(pid, index, size);

		// Signals semaphore
		pthread_mutex_unlock(sem);
	}
}

// Does all the critical section stuff, including sleeping & writing to file
static void criticalSection(pid_t pid, int index, int size){
	FILE * logFile;		// The protected log file
	char msg[BUFF_SZ];	// Buffer for stderr and log file message

	// Creates critical section message and prints to stderr
	sprintf(msg, "Process %d in critical section\n", pid);
	fprintf(stderr, msg);
	
	// Attempts to open log file
	if ((logFile = fopen(LOG_FILE_NAME, "a")) == NULL)
		perrorExit("Couldn't open log file");

	// Sleeps, writes to log file, sleeps again
	sleep(PRE_LOG_SLEEP);
	fprintf(logFile, msg);
	fprintf(logFile, "%d %d %d\n", (int)pid, index, size);
	sleep(POST_LOG_SLEEP);

	// Closes file
	fclose(logFile);
}

// Stores sum of ints from resultIndex to resultIndex + numInts at resultIndex
static void sumInts(int * intArray, int resultIndex, int numInts){
	int i = 1;
	for (; i < numInts; i++){
		intArray[resultIndex] += intArray[resultIndex + i];
	}
}

// Shifts results left so that they are contiguous
static void leftShiftInts(int * intArray, int size, int gap){
        int left = 1;     // Index of an int on the left
        int right = gap;  // Index of an int on the right

	printf("BEFORE LEFTSHIFT:\n");
	printArray(intArray, 32);

        while (right < size){
                intArray[left] = intArray[right];
                left++;
                right += gap;
        }

	// Appends a 0 so method 1 works with odd values of ceil(n/lg(n)) 
	intArray[left] = 0;

	printf("AFTER LEFTSHIFT:\n");
	printArray(intArray, 32);
}

