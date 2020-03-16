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

/* Prototypes */
static void launchChildren(char * argv[], int size, int numGroups, int gap);
static pid_t launchChild(char * argv[], int index, int size);
static void updateLogFile(pid_t pid, int index, int size);
static void criticalSection(pid_t pid, int index, int size);
static void sumInts(int * intArray, int resultIndex, int numInts);
static void leftShiftInts(int * intArray, int numInts, int gap);
static void logSemaphoreActivity(char * msg);

/* Static Global Variables */
static char * shm = NULL;       	  // Pointer to shared memory region
static pthread_mutex_t * sem = NULL;      // Semaehore protecting logFile
static pthread_mutex_t * semLgSem = NULL; // Sem protecting sem activity log

int main(int argc, char * argv[]){
	int * intArray;			// Pointer to the shared int array

	int pid = getpid();		// Pid of the current process
	int index = atoi(argv[1]);	// Index of the current process
	int size = atoi(argv[2]);	// Number of ints process should add
	int shmSize = atoi(argv[3]);	// Size of the shared memory region

	exeName = argv[0];

	// Gets pointers to shared memory items
	shm = sharedMemory(shmSize, 0);
	sem = (pthread_mutex_t *)(shm);
	semLgSem = (pthread_mutex_t *)(shm + sizeof(pthread_mutex_t));
	intArray = (int *)(shm + 2 * sizeof(pthread_mutex_t));

	// Launches children if called with -1 or -2 as an index by master
	if (index < 0){
		int numGroups; // The number of groups of ints to add
		int groupSize; // The number of ints per group

		// Computes values for method 1
		if (index == -1) {
			numGroups = (int)ceil(size/2.0);
			groupSize = 2;

		// Computes values for method 2
		} else if (index == -2) {
			numGroups = \
				(int)ceil(size/log((double)size)/log(2.0));
			groupSize = (int)ceil(log((double)size)/log(2.0));
		}

		// Creates child bin_adders
		launchChildren(argv, size, numGroups, groupSize);

		// Performs computation as parent bin_adder
		sumInts(intArray, 0, groupSize);
		leftShiftInts(intArray, size, groupSize);

	// Performs computation if this process is a child of a bin_adder		
	} else {
		sumInts(intArray, index, size);
	}
	
	// Writes to log, accessing critical section at most 5 times
	updateLogFile(pid, index, size);
	
	return 0;

}

// Launches numGroups-1 children which each sum numInts integers & store @ index
static void launchChildren(char * argv[], int size, int numGroups, int numInts){
	int index = numInts;	 // Stores index of each child process
	int running = 0;	 // The number of children currently executing
	int completed = 0;	 // The number of children that finished
	pid_t pid = 0;		 // Temp storage for child pids

	while (running + completed < numGroups){

		// Checks for numInts change on last iteration for method 2
		if (running + completed == numGroups - 1)
			numInts = max(size - numGroups * numInts, 2);

		// Creates bin_adder child with new index and size
		launchChild(argv, index, numInts);

		// Updates index and number of running children	
		index += numInts;
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

	// Waits for running child processes to finish
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
	char msgBuff[BUFF_SZ];
	
	// Seeds random number generator
	srand((unsigned int) time(NULL) + pid);	
	
	// Follows the template provided in the assignment description
	int i;
	for (i = 0; i < 5; i++){

		// Sleeps for random ammount of time (between 0 and 3 seconds)	
		sleep(random() % (MAX_SLEEP - MIN_SLEEP + 1) + MIN_SLEEP);

		// Prints semaphore activity to stderr and log
		current_time = time(NULL);
		sprintf(msgBuff, 
			"%d %d %d waiting for semaphore before critical" \
			" section %s",(int)pid, index, size, \
			ctime(&current_time)
		);
		fprintf(stderr, msgBuff);
		logSemaphoreActivity(msgBuff);

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

	// Creates critical section message and prints to stderr and sem_log
	sprintf(msg, "%d %d %d semaphore aquired, in critical section\n", pid,
		 index, size);
	fprintf(stderr, msg);
	logSemaphoreActivity(msg);
	
	// Attempts to open log file
	if ((logFile = fopen(LOG_FILE_NAME, "a")) == NULL)
		perrorExit("Couldn't open log file");

	// Sleeps, writes to log file, sleeps again
	sleep(PRE_LOG_SLEEP);
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

        while (right < size){
                intArray[left] = intArray[right];
                left++;
                right += gap;
        }

	// Appends a 0 so method 1 works with odd values of ceil(n/lg(n)) 
	intArray[left] = 0;

}

// Logs when process waits for or aquires a semaphore
static void logSemaphoreActivity(char * msg){
	FILE * semLog;

	// Protects access to semaphore log file
	pthread_mutex_lock(semLgSem);

	// Writes message to semaphore log file
	if ((semLog = fopen(SEM_LOG_NAME, "a")) == NULL)
		perrorExit("Couldn't open semaphore log file");

	fprintf(semLog, msg);

	if (fclose(semLog) == -1)
		perrorExit("Failed to close semaphore log file");


	// Signals semaphore log semaphore
	pthread_mutex_unlock(semLgSem);
}

