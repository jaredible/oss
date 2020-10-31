#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shared.h"

typedef struct {
	unsigned int quantum;
	bool outputDigest;
	unsigned int timeout;
} Options;

typedef struct {
	unsigned int quantum;
	Options options;
	Shared *shared;
} Global;

void initializeProgram(int, char**);
void simulateOS();
void scheduleProcess(PCB*, Message*);
void cleanupResources(bool);

void addSystemTime(int, int);
Time *getSystemTime();

static Global *global = NULL;

int main(int argc, char **argv) {
	global = (Global*) malloc(sizeof(Global));
	initializeProgram(argc, argv);
	simulateOS();
	cleanupResources(false);
	return EXIT_SUCCESS;
}

void initializeProgram(int argc, char **argv) {
	init(argc, argv);
	
	void signalHandler(int signal) {
		kill(0, SIGUSR1);
		while (wait(NULL) > 0);
		cleanupResources(true);
		exit(EXIT_FAILURE);
	}
	
	signal(SIGINT, signalHandler);
	
	void usage(int status) {
		if (status != EXIT_SUCCESS) fprintf(stderr, "Try '%s -h' for more information\n", getProgramName());
		else {
			printf("NAME\n");
			printf("       %s - operating system simulator\n", getProgramName());
			printf("USAGE\n");
			printf("       %s -h\n", getProgramName());
			printf("       %s [-q x] [-d]\n", getProgramName());
			printf("DESCRIPTION\n");
			printf("       -h       : Prints usage information and exits\n");
			printf("       -q x     : Base quantum for queues (default %d)\n", (int) QUANTUM_BASE_DEFAULT);
			printf("       -d       : Outputs file with digest of process scheduling\n");
			printf("       -t time  : Time, in seconds, after which the program will terminate (default %d)\n", (int) TIMEOUT_DEFAULT);
		}
		exit(status);
	}
	
	bool ok = true;
	int quantum = QUANTUM_BASE_DEFAULT;
	bool outputDigest = DIGEST_OUTPUT_DEFAULT;
	int timeout = TIMEOUT_DEFAULT;
	
	while (true) {
		int c = getopt(argc, argv, "hq:t:d");
		if (c == -1) break;
		switch (c) {
			case 'h':
				usage(EXIT_SUCCESS);
			case 'q':
				if (!isdigit(*optarg) || (quantum = atoi(optarg)) <= QUANTUM_BASE_MIN) {
					error("invalid base quantum '%s'", optarg);
					ok = false;
				}
				break;
			case 't':
				if (!isdigit(*optarg) || (timeout = atoi(optarg)) <= TIMEOUT_MIN) {
					error("invalid timeout '%s'", optarg);
					ok = false;
				}
				break;
			case 'd':
				outputDigest = true;
				break;
			default:
				ok = false;
		}
	}
	
	if (optind < argc) {
		char buf[BUFFER_LENGTH];
		snprintf(buf, BUFFER_LENGTH, "found non-option(s): ");
		while (optind < argc) {
			strncat(buf, argv[optind++], BUFFER_LENGTH);
			if (optind < argc) strncat(buf, ", ", BUFFER_LENGTH);
		}
		error(buf);
		ok = false;
	}
	
	if (!ok) usage(EXIT_FAILURE);
	
	global->options.quantum = quantum;
	global->options.outputDigest = outputDigest;
	global->options.timeout = timeout;
	
	void timer(unsigned int seconds) {
		signal(SIGALRM, signalHandler);
		
		struct itimerval val;
		val.it_value.tv_sec = seconds;
		val.it_value.tv_usec = 0;
		val.it_interval.tv_sec = 0;
		val.it_interval.tv_usec = 0;
		if (setitimer(ITIMER_REAL, &val, NULL) == -1) crash("setitimer");
	}
	
	timer(timeout);
}

void simulateOS() {
}

void scheduleProcess(PCB *pcb, Message *message) {
}

void cleanupResources(bool forced) {
}
