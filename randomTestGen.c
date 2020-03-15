// randomTestGen.c was created by Mark Renard on 3/13/2020
// 
// This file contains a program which writes NUM_INTS integers in the range
// [MIN, MAX] to a file on consecutive new lines to be used as test
// files by master

#include <time.h>
#include <stdlib.h>
#include <stdio.h>

const int NUM_INTS = 32;	 // The number of integers to write to the file
const int MIN = 0;		 // The minimum of the range of values of ints
const int MAX = 255;		 // The maximum of the range of values of ints
const char * FILE_NAME = "test"; // The name of the output file

FILE * outFile;	// Pointer to the output file

int main(int argc, char * argv[]){
	int randomInt;
	int sum = 0;
	
	// Opens the output file in write mode
	if ((outFile = fopen(FILE_NAME, "w+")) == NULL){
		char buff[100];
		sprintf(buff, "%s: Error: Couldn't open outFile", argv[0]);
		perror(buff);
		exit(1);
	}	

	// Seeds random number generator
	srandom((unsigned int) time(NULL));

	// Writes the integers
	int i;
	for (i = 0; i < NUM_INTS; i++){
		randomInt = argc > 1 ? 1 \
			 : random() % (MAX - MIN + 1) + MIN;
			
		fprintf(outFile, "%d\n", randomInt);
		sum += randomInt;
	}	

	printf("Random int sum: %d", sum);

	return 0;
}
