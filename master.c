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
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include "perrorExit.h"
#include "sharedMemory.h"
#include "shmkey.h"
#include "semname.h"

/* Prototypes */
static void assignSignalHandlers();
static void cleanUpAndExit(int param);
static int numberOfIntegers(FILE * inFile);
static void copyIntegersFromFile(FILE * inFile, int * integers, int numInts);
//static void launchChildren(int * integers, int numInts);
static void cleanUp();

/* Named Constants */
static const int BUFF_SZ = 100;		 // Number of bytes in char buffers
static const int MAX_SECONDS = 100;	 // Maximum total execution time
static const char * LOG_FILE_NAME = "adder_log"; // Name of shared log file

/* Static Global Variables */
static char * shm = NULL;	 // Pointer to the shared memory region
static FILE * inFile = NULL;	 // The file with integers to read
FILE * logFile = NULL;		 // Pointer to log file in shared memory
static sem_t * sem = NULL;	 // Semaphore protecting access to logFile


int main(int argc, char * argv[]){
	unsigned int numInts;	 // The number of integers to read from input
	int * intArray;		 // Pointer to the first int in the shared array

	alarm(MAX_SECONDS);	 // Limits total execution time to MAX_SECONDS

	exeName = argv[0];	 // Assigns executable name for perrorExit
	assignSignalHandlers();	 // Determines response to ctrl + C & alarm

	// Opens inFile with specified path, exits on failure
	if (argc < 2) perrorExit("Please specify input file name");
	if ((inFile = fopen(argv[1], "r")) == NULL)
		perrorExit("Couldn't open input file");

	// Counts the number of integers in the input file
	numInts = numberOfIntegers(inFile);

	// Allocates shared memory for log file and integers
	shm = sharedMemory(sizeof(FILE*) + numInts * sizeof(int), IPC_CREAT);

	// Sets addresses of shared log file and integer array
	logFile = (FILE*)shm;
	intArray = (int*)(shm + sizeof(FILE*));	
		
	// Opens the shared log file
	if ((logFile = fopen(LOG_FILE_NAME, "w+")) == NULL)
		perrorExit("Couldn't open log file");

	// Initializes semaphore to provide mutual exclusion fof logFile access
	if ((sem = sem_open(SEMNAME, O_CREAT, 0600, 1)) == SEM_FAILED)
		perrorExit("Failed creating semaphore");

	// Copies ints from file into shared integer array
	copyIntegersFromFile(inFile, intArray, numInts);
	
	// Launch children
	// launchChildren(numInts, intArray);
	
	cleanUp();

	return 0;
}

// Determines the processes response to ctrl + c or alarm
static void assignSignalHandlers(){
        struct sigaction sigact;

	// Initializes sigaction values
        sigact.sa_handler = cleanUpAndExit;
        sigact.sa_flags = 0;

	// Assigns signals to sigact
        if ((sigemptyset(&sigact.sa_mask) == -1)
	    ||(sigaction(SIGALRM, &sigact, NULL) == -1)
	    ||(sigaction(SIGINT, &sigact, NULL)  == -1))
		perrorExit("Faild to install signal handler");

}

// Signal handler that deallocates shared memory, terminates children, and exits
void cleanUpAndExit(int param){

        // Prints error message
        char buff[BUFF_SZ];
        sprintf(buff,
                 "%s: Error: Terminating after receiving a signal",
                 exeName
        );
        perror(buff);

	// Cleans up and exits
	cleanUp();
        exit(1);
}

// Closes files, removes shared memory, and kills child processes
static void cleanUp(){

	// Closes shared log file
	if (logFile != NULL) fclose(logFile);

	// Detatches from and removes shared memory
	detach(shm);
	removeSegment();

	// kills all other processes in the same process group
	signal(SIGQUIT, SIG_IGN);
	kill(0, SIGQUIT);
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

// Copies the integers from the input file to the shared memory array
static void copyIntegersFromFile(FILE * inFile, int * integers, int numInts){
	char ch;		// Stores each char from inFile
	char buff[BUFF_SZ];	// Buffers chars to be converted to ints
	int buffIndex = 0;	// Index of next buffer char
	int intIndex = 0;	// Index of next integer in the array

	// Resets file descriptor to beginning of file
	rewind(inFile);

	// Converts each line to an integer and stores in the shared int array
	while ((ch = fgetc(inFile)) != EOF){
	
		// Handles any error from fgetc
		if (ferror(inFile))
			perrorExit("copyIntegersFromFile couldn't read char");
		
		// Adds digits to buff
		if (isdigit(ch)){
			buff[buffIndex++] = ch;

			// Error on buffer overflow
			if (buffIndex + 2 > BUFF_SZ)
				perrorExit(
					"copyIntegersFromFile buffer overflow"
				);
	
		// Converts buff to int and adds to shared array on \n
		} else if (ch == '\n') {

			// intIndex should never be out of bounds
			if (intIndex >= numInts)
				perrorExit(
					"copyIntegersFromFile out of bounds!"
				);

			// Adds new int to array 
			buff[buffIndex] = '\0'; // Null terminates buff
			integers[intIndex++] = atoi(buff); // Adds new int

			// Resets buffer
			buffIndex = 0;
			buff[0] = '\0';
		}
	}
	
	// Prints the integers in shared memory
	int i;
	printf("Integers: ");
	for (i = 0; i < numInts; i++)
		printf("%d, ", integers[i]);
	printf("\n");
}

