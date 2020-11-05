#ifndef OSS_H
#define OSS_H

#include <stdio.h>

#include "shared.h"

//#define PROCESSES_CONCURRENT_MAX 2 // TODO: rename?
//#define PROCESSES_TOTAL_MAX 100 // TODO: rename?

#define QUANTUM_BASE_MIN 1e3
#define QUANTUM_BASE_MAX 1e5
#define QUANTUM_BASE_DEFAULT 1e4

#define TIMEOUT_MIN 1
#define TIMEOUT_MAX 10
#define TIMEOUT_DEFAULT 3

#define OPTIONS_TIMEOUT_DEFAULT TIMEOUT_DEFAULT
#define OPTIONS_VERBOSE_DEFAULT false

//#define QUEUE_SET_COUNT 4
//#define QUEUE_SET_SIZE PROCESSES_CONCURRENT_MAX
//#define QUEUE_BLOCK_SIZE PROCESSES_CONCURRENT_MAX

void usage(int status) {
	if (status != EXIT_SUCCESS) fprintf(stderr, "Try '%s -h' for more information\n", getProgramName());
	else {
		printf("NAME\n");
		printf("       %s - OS process-scheduling simulator\n", getProgramName());
		printf("USAGE\n");
		printf("       %s -h\n", getProgramName());
		printf("       %s [-q x] [-t time] [-d] [-v]\n", getProgramName());
		printf("DESCRIPTION\n");
		printf("       -h       : Prints usage information and exits\n");
		printf("       -t time  : Time, in seconds, after which the program will terminate (default %d)\n", OPTIONS_TIMEOUT_DEFAULT);
	}
	exit(status);
}

void timer(unsigned int seconds) {
	struct itimerval val;
	val.it_value.tv_sec = seconds;
	val.it_value.tv_usec = 0;
	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &val, NULL) == -1) crash("setitimer");
}

#endif
