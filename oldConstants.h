// constants.h was created by Mark Renard on 3/14/2020
//
// This file contains a number of constants used in OS assignment 3

#ifndef CONSTANTS_H
#define CONSTANTS_H

/* Used by master.c */
const int MAX_SECONDS = 100;		  // Max total execution time
const char * LOG_FILE_NAME = "adder_log"; // Name of the log file

/* Used by bin_adder.c */
const int MAX_PROCESSES = 20;		     // Max simultaneous processes
const int MAX_RUNNING = (MAX_PROCESSES - 2); // Max children of bin_adder

const int MAX_SLEEP = 3;	// Max sleep in seconds before critical section
const int MIN_SLEEP = 0;	// Min sleep in seconds before critical section

const int PRE_LOG_SLEEP = 1;	// Sleep time just before writing to log
const int POST_LOG_SLEEP = 1;	// Sleep time just after writing to log

/* Used by both */
const int BUFF_SZ 100;		 	 // The size of character buffers
const char * CHILD_PATH	= "./bin_adder"; // Path to child processes

#endif
