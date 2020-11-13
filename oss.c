/*
 * oss.c 11/17/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#include <getopt.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <wait.h>

#include "ipc.h"
#include "oss.h"
#include "queue.h"
#include "shared.h"

static bool verbose = false;
static int n = TOTAL_PROCS_MAX;
static int s = CONCURRENT_PROCS_MAX;
static int t = TIMEOUT_MAX;

static volatile bool stop = false;

static Message message;
static int spawnedProcessCount = 0;
static int exitedProcessCount = 0;

static bitvector bv;

int main(int argc, char **argv) {
	init(argc, argv);
	
	signal(SIGINT, sighandler);
	signal(SIGALRM, sighandler);
	
	bool ok = true;
	
	while (true) {
		int c = getopt(argc, argv, OPTIONS);
		if (c == -1) break;
		switch (c) {
			case 'h':
				usage(EXIT_SUCCESS);
			case 'v':
				verbose = true;
				break;
			case 'n':
				if (!isdigit(*optarg) || (n = atoi(optarg)) < TOTAL_PROCS_MIN) {
					error("invalid total processes '%s'", optarg);
					ok = false;
				}
				break;
			case 's':
				if (!isdigit(*optarg) || (s = atoi(optarg)) < CONCURRENT_PROCS_MIN) {
					error("invalid concurrent processes '%s'", optarg);
					ok = false;
				}
				break;
			case 't':
				if (!isdigit(*optarg) || (t = atoi(optarg)) < TIMEOUT_MIN) {
					error("invalid timeout '%s'", optarg);
					ok = false;
				}
				break;
			default:
				ok = false;
		}
	}
	
	if (optind < argc) {
		char buf[BUFFER_LENGTH];
		snprintf(buf, BUFFER_LENGTH, "found non-options(s): ");
		while (optind < argc) {
			strncat(buf, argv[optind++], BUFFER_LENGTH);
			if (optind < argc) strncat(buf, ", ", BUFFER_LENGTH);
		}
		error(buf);
		ok = false;
	}
	
	if (!ok) usage(EXIT_FAILURE);
	
//	timer(t);
	
	int id = 0;
	Time *system = (Time*) shm_create(sizeof(Time), &id);
	system->s = 69;
	system->ns = 420;
	printf("[%03d:%03d] id: %d\n", system->s, system->ns, id);
	shm_detach(system);
	shm_remove(0);
	return 0;
	
	simulate();
	
	cleanup();
	
	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

void usage(int status) {
	if (status != EXIT_SUCCESS) fprintf(stderr, "Try '%s -h' for more information\n", getProgramName());
	else {
		printf("NAME\n");
		printf("USAGE\n");
		printf("DESCRIPTION\n");
	}
	exit(status);
}

void timer(int seconds) {
	struct itimerval val;
	val.it_value.tv_sec = seconds;
	val.it_value.tv_usec = 0;
	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &val, NULL) == -1) crash("setitimer");
}

void sighandler(int signum) {
	if (stop) return;
	if (signum == SIGINT || signum == SIGALRM) stop = true;
}

void cleanup() {
}

void simulate() {
	srand(0);
	
	while (true) {
		trySpawn();
		
		int status;
		pid_t pid = waitpid(-1, &status, WNOHANG);
		if (pid > 0) {
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) > EXIT_STATUS_OFFSET) {
					int localPID = WEXITSTATUS(status) - EXIT_STATUS_OFFSET;
					printf("Exited: %d\n", localPID);
				}
			}
		}
		
		if (!canSimulate()) break;
		break; // TODO: remove
	}
}

bool canSimulate() {
	return !stop || exitedProcessCount < spawnedProcessCount;
}

void trySpawn() {
	if (canSpawn()) spawn();
}

bool canSpawn() {
	return !stop && spawnedProcessCount < n;
}

void spawn() {
	int localPID = findAvailablePID();
	if (localPID > -1) {
		BIT_SET(bv, localPID - 1);
		
		pid_t pid = fork();
		if (pid == -1) crash("fork");
		else if (pid == 0) {
		}
		
		initProcess();
		onProcessCreated();
	}
}

void initProcess() {
}

void onProcessCreated() {
}

void onProcessRequested() {
}

void onProcessReleased() {
}

void onProcessTerminated() {
}

void onProcessExited() {
}

int findAvailablePID() {
	unsigned i = 1, pos = 1;
	while (i & bv && pos <= s) {
		i <<= 1;
		pos++;
	}
	return pos <= s ? pos : -1;
}

bool isSafe() {
	return true;
}

void displayVector() {
}

void displayMatrix() {
}
