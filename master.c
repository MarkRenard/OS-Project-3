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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>

#include "perrorExit.h"
#include "sharedMemory.h"
#include "shmkey.h"
#include "semname.h"
#include "constants.h"

/* Prototypes */
static void assignSignalHandlers();
static void cleanUpAndExit(int param);
static int numberOfIntegers(FILE * inFile);
static void copyIntegersFromFile(int * intArray, int numInts);
static void launchChildren(int * intArray, int numInts, int shmSize);
static pid_t createChild(int index, int numInts, int shmSize);
static void cleanUp();
static void printArray(int * array, int size);
static void leftShiftInts(int * intArray, int numInts, int gap);
static void initializeSemaphore(pthread_mutex_t *);

/* Static Global Variables */
static char * shm = NULL;	 // Pointer to the shared memory region
static FILE * inFile = NULL;	 // The file with integers to read
static FILE * logFile = NULL; 	 // Pointer to log file in shared memory
static pthread_mutex_t * sem;	 // Semaphore protecting logFile

int main(int argc, char * argv[]){
	unsigned int numInts;	 // The number of integers to read from input
	int * intArray;		 // Pointer to the first int in the shared array
	int shmSz;		 // The size of the shared memory region in bytes

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
	shmSz = sizeof(FILE*) + sizeof(pthread_mutex_t) + numInts * sizeof(int);
	shm = sharedMemory(shmSz, IPC_CREAT);

	// Sets addresses of shared log file, a semaphore, and the integer array
	logFile = (FILE*)shm;
	sem = (pthread_mutex_t*)(shm + sizeof(FILE*));
	intArray = (int*)(shm + sizeof(FILE*) + sizeof(pthread_mutex_t));	
		
	// Opens the shared log file
	if ((logFile = fopen(LOG_FILE_NAME, "w")) == NULL)
		perrorExit("Couldn't open log file");
	
	fprintf(logFile, "Testing %d %d %d\n", 1, 2, 3);

	// Initializes semaphore to provide mutual exclusion for logFile access
	initializeSemaphore(sem);

	// Copies ints from file into shared integer array
	copyIntegersFromFile(intArray, numInts);
	
	// Launches children
	launchChildren(intArray, numInts, shmSz);
	
	// Ignores interrupts, kills child processes, closes files, removes shm
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

// Signal handler - closes files, removes shm, terminates children, and exits
void cleanUpAndExit(int param){

	// Closes files, removes shm, terminates children
	cleanUp();

        // Prints error message
        char buff[BUFF_SZ];
        sprintf(buff,
                 "%s: Error: Terminating after receiving a signal",
                 exeName
        );
        perror(buff);

	// Exits
        exit(1);
}

// Ignores interrupts, kills child processes, closes files, removes shared mem
static void cleanUp(){

	// Handles multiple interrupts by ignoring until exit
	signal(SIGALRM, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	// Kills all other processes in the same process group
	kill(0, SIGQUIT);

	// Closes files
	if (logFile != NULL) fclose(logFile);
	if (inFile != NULL) fclose(inFile);

	// Detatches from and removes shared memory
	detach(shm);
	removeSegment();
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
static void copyIntegersFromFile(int * intArray, int numInts){
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
			intArray[intIndex++] = atoi(buff); // Adds new int

			// Resets buffer
			buffIndex = 0;
			buff[0] = '\0';
		}
	}
	
	// Prints the intArray in shared memory
	printArray(intArray, numInts);
	printf("\n");
}

// Initializes semaphore protecting the log file
static void initializeSemaphore(pthread_mutex_t * mutex){
        pthread_mutexattr_t attributes;	// mutex attributes struct

	// Initializes mutex attributes struct
        pthread_mutexattr_init(&attributes);

	// Specifies that the semaphore can be used by multiple processes
        pthread_mutexattr_setpshared(&attributes, PTHREAD_PROCESS_SHARED);

	// Initializes the mutex with the attributes struct
        pthread_mutex_init(mutex, &attributes);
}

// Launches a bin_adder child for each iteration of the summation algorithm
static void launchChildren(int * intArray, int numInts, int shmSize){
	int intsToAdd;	// The number of integers for the child to add
	pid_t pid;	// Pid of each child process launch children creates
	
	printf("Running launchChildren - numInts: %d\n", numInts);
	fflush(stdout);

	intsToAdd = numInts;
	while(intsToAdd > 1){
		pid = createChild(0, intsToAdd, shmSize);
		waitpid(pid, NULL, 0);

		printf("Before shift: ");
		printArray(intArray, intsToAdd);

		// Moves integers to the left
		leftShiftInts(intArray, intsToAdd, 2);
		intsToAdd /= 2;

		printf("After shift: ");
		printArray(intArray, intsToAdd);
		printf("\n");
		
		//sleep(1);
	}

}

static void leftShiftInts(int * intArray, int numInts, int gap){
	int left = 1;	 // Index of an int on the left
	int right = gap; // Index of an int on the right

	while (right < numInts){
		intArray[left] = intArray[right];
		left++;
		right += gap;
	}
}

static void printArray(int * array, int size){
	int i;
	for (i = 0; i < size; i++){
		printf("%d, ", array[i]);
	}
	printf("\n");
}
	

static pid_t createChild(int index, int numInts, int shmSize){
	pid_t pid;

	printf("\nCreate child called for index %d, numInts %d\n", index, numInts);
	fflush(stdout);

	// Execs bin_adder if this process is the child
	if ((pid = fork()) == -1) perrorExit("createChild failed to fork");

	if(pid == 0) {
		char indx[BUFF_SZ];
		sprintf(indx, "%d", index);

		char nInts[BUFF_SZ];
		sprintf(nInts, "%d", numInts);

		char shmSz[BUFF_SZ];
		sprintf(shmSz, "%d", shmSize);

		execl("./bin_adder", "./bin_adder", indx, nInts, shmSz, NULL);
		perrorExit("Failed to exec!");

	}
	
	// Returns pid of child if this process is parent
	return pid;	
		
}

