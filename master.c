// master.c was created on 3/10/2020
//
// This file contains a program which reads integers from a file into a shared
// memory array, divides them into groups, creates children that sum each group
// and append comments to a log file, and measures the performance of the
// processes.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "perrorExit.h"
#include "sharedMemory.h"
#include "shmkey.h"


static void assignSignalHandlers();
static int numberOfIntegers(FILE * inFile);
//static void initalizeMutex(pthread_mutex_t*);
static void copyIntegersFromFile(FILE * inFile, int * integers, int numInts);
//static void launchChildren(int * integers, int numInts);
static void cleanUp();

static const int BUFF_SZ = 100;	 // Number of bytes in char buffers

static char * shm = NULL;	 // Pointer to the shared memory region
static FILE * inFile = NULL;	 // The file with integers to read
FILE * logFile = NULL;		 // Pointer to log file in shared memory

int main(int argc, char * argv[]){
	unsigned int numInts;	 // The number of integers to read from input
	pthread_mutex_t * mutex; // Pointer to the mutex that protects the file
	int * integers;		 // Pointer to the first int in the shared array

	exeName = argv[0];	 // Assigns executable name for perrorExit
	assignSignalHandlers();	 // Determines response to ctrl + C & alarm

	// Opens inFile with specified path, exits on failure
	if (argc < 2) perrorExit("Please specify input file name");
	if ((inFile = fopen(argv[1], "r")) == NULL)
		perrorExit("Couldn't open input file");

	// Counts the number of integers in the input file
	numInts = numberOfIntegers(inFile);

	// Allocates shared memory for log file, a semaphore, and integers
	shm = sharedMemory(
		sizeof(FILE*) + sizeof(pthread_mutex_t) + numInts * sizeof(int),
		IPC_CREAT
	);

	// Sets addresses of shared log file, semaphore, and integer array
	logFile = (FILE*)shm;
	mutex = (pthread_mutex_t*)(shm + sizeof(FILE*));
	integers = (int*)(mutex + sizeof(pthread_mutex_t));	
		
	// Opens the shared log file
	if ((logFile = fopen("bin_adder.log", "w+")) == NULL)
		perrorExit("Couldn't open log file");

	// Initializes semaphore
	// initializeMutex(mutex);

	// Copies ints from file into shared integer array
	copyIntegersFromFile(inFile, integers, numInts);
	
	// Launch children
	
	cleanUp();

	return 0;
}

// Determines the processes response to ctrl + c or alarm
static void assignSignalHandlers(){
	printf("assignSignalHandlers - doing nothing for now\n");
	fflush(stdout);
}

// Counts the integers and validates the file format or exits
static int numberOfIntegers(FILE * inFile){
	int line = 1;		// The line number of the file
	int newLine = 1;	// 1 if at the beginning of a new line
	int numIntegers = 0;	// Counter for the number of integers
	char ch;		// Stores each char in file

	while ((ch = fgetc(inFile)) != EOF){
		if (ferror(inFile)) perrorExit("Error counting integers");

		// Sets newline to true if \n, error on consecutive \n
		if (ch == '\n'){
   			if (!newLine){
				newLine = 1;
				line++;
			} else {
				char buff[BUFF_SZ];
				sprintf(buff, "Line %d: consecutive \\n", line);
				errno = EPERM;
				perrorExit(buff);
			}

		// Increments numIntegers on digit after new line
 		} else if (newLine && isdigit(ch)){
			newLine = 0;
			numIntegers++;

		// Exits if non-digit found
		} else if (!isdigit(ch)){
			char buff[BUFF_SZ];
			sprintf(buff, "Non-int on line %d\n", line);
			errno = EPERM;
			perrorExit(buff);
		}
	}

	printf("Num integers: %d\n", numIntegers);

	return numIntegers;
}

// Initializes semaphore protecting the log file


// Copies the integers from the input file to the shared memory array
static void copyIntegersFromFile(FILE * inFile, int * integers, int numInts){
	char * line = NULL;
	size_t size;

	// Resets file descriptor to beginning of file
	rewind(inFile);

	// Converts each line to an integer and stores in the shared int array
	int i;
	for(i = 0; i < numInts; i++){
		if ((getline(&line, &size, inFile)) == -1)
			free(line);
			perrorExit("Failed to read line from input");

		// Copies each integer into the array
		integers[i] = atoi(line);
	}

	// Prints the integers in shared memory
	printf("Integers: ");
	for (i = 0; i < numInts; i++)
		printf("%d, ", integers[i]);

	free(line);
}

// Closes files, kills child processes and removes shared memory segment
static void cleanUp(){
	// Sends kill signal to child processes
	// (eventually)

	// Closes open files
	if (inFile != NULL) fclose(inFile);
	if (logFile != NULL) fclose(logFile);
		
	// Detaches from and removes shared memory	
	detach(shm);
	removeSegment();
}	
