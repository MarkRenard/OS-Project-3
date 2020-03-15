// constants.h was created by Mark Renard on 3/14/2020
//
// This file contains a number of constants used in OS assignment 3

#ifndef CONSTANTS_H
#define CONSTANTS_H

/* Used by master.c */
#define MAX_SECONDS 100			// Max total execution time
#define LOG_FILE_NAME "adder_log" 	// Name of the log file

/* Used by bin_adder.c */
#define MAX_PROCESSES 20		// Max simultaneous processes
#define MAX_RUNNING MAX_PROCESSES - 2	// Max children of bin_adder

#define MAX_SLEEP 3	// Max sleep in seconds before critical section
#define MIN_SLEEP 0	// Min sleep in seconds before critical section

#define PRE_LOG_SLEEP 1		// Sleep time just before writing to log
#define POST_LOG_SLEEP 1	// Sleep time just after writing to log

/* Used by both */
#define BUFF_SZ 100			// The size of character buffers
#define CHILD_PATH "./bin_adder"	// Path to child processes

#endif
