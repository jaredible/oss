#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "queue.h"
#include "shared.h"

typedef struct {
	unsigned int quantum;
	bool outputDigest;
	unsigned int timeout;
} Options;

typedef struct {
	Options options;
	Shared *shared;
	Queue *qset1[QUEUE_RUN_COUNT];
	Queue *qset2[QUEUE_RUN_COUNT];
	Queue **active;
	Queue **expired;
	Queue *blocked;
	unsigned long int vec;
	Message *message;
	unsigned int spawnedProcessCount;
} Global;

void initializeProgram(int, char**);
void simulateOS();
bool canSpawnProcess();
void trySpawnProcess();
void spawnProcess();
void initializePCB(PCB*, unsigned int, pid_t);
void handleRunningProcess();
void handleBlockedProcesses();
void tryScheduleProcess();
void scheduleProcess(PCB*);
void cleanupResources(bool);
void handleSignal(int);

int findAvailableLocalPID();
PCB *getPCB(unsigned int);
bool isProcessRunning();

void initializeQueues();
void swapRunSets();
Queue **getActiveSet();
void setActiveSet(Queue**);
Queue **getExpiredSet();
void setExpiredSet(Queue**);
Queue *getBlocked();

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
	
	signal(SIGINT, handleSignal);
	
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
		signal(SIGALRM, handleSignal);
		
		struct itimerval val;
		val.it_value.tv_sec = seconds;
		val.it_value.tv_usec = 0;
		val.it_interval.tv_sec = 0;
		val.it_interval.tv_usec = 0;
		if (setitimer(ITIMER_REAL, &val, NULL) == -1) crash("setitimer");
	}
	
	timer(timeout);
	
	allocateSharedMemory(true);
	allocateMessageQueues(true);
	
	global->shared = getSharedMemory();
	global->shared->os.quantum = quantum;
}

void simulateOS() {
	global->message = (Message*) malloc(sizeof(Message));
	
	initializeQueues();
	trySpawnProcess();
	while (true) {
		// TODO: increment time

		//trySpawnProcess();
		handleRunningProcess();
		handleBlockedProcesses();
		tryScheduleProcess();
		
		int status;
		pid_t pid = waitpid(-1, &status, WNOHANG);
		if (pid > 0) {
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) == 21) {
					// TODO
					break;
				}
			}
		}
		
		if (false) {
			break;
		}
	}
}

bool canSpawnProcess() {
	return true;
}

void trySpawnProcess() {
	if (canSpawnProcess()) {
		spawnProcess();
	}
}

void spawnProcess() {
	int localPID = findAvailableLocalPID();
	if (localPID > -1) {
		pid_t pid = fork();
		if (pid == -1) crash("fork");
		else if (pid == 0) {
			char buf[BUFFER_LENGTH];
			snprintf(buf, BUFFER_LENGTH, "%d", localPID);
			execl("./user", "user", buf, (char*) NULL);
			crash("execl");
		}
		
		PCB *pcb = getPCB(localPID);
		initializePCB(pcb, localPID, pid);
		queue_push(getActiveSet()[pcb->priority], localPID);
		logger("[CREATED] Priority: %d, PID: %d", pcb->priority, localPID);
	}
}

void initializePCB(PCB *pcb, unsigned int localPID, pid_t actualPID) {
	pcb->priority = 0;
	pcb->localPID = localPID;
	pcb->actualPID = actualPID;
}

void handleRunningProcess() {
	if (isProcessRunning()) {
		PCB *pcb = global->shared->running;
		receiveMessage(global->message, getParentQueue(), pcb->actualPID, true);
		if (strcmp(global->message->text, "TERMINATED") == 0) {
			// TODO: clear bit-vector bit
			logger("[TERMINATED] PID: %d", pcb->localPID);
			global->shared->running = NULL;
		} else if (strcmp(global->message->text, "EXPIRED") == 0) {
			logger("[EXPIRED] PID: %d %d->%d", pcb->localPID, 0, 0);
			queue_push(getActiveSet()[pcb->priority], pcb->localPID);
		} else if (strcmp(global->message->text, "BLOCKED") == 0) {
		}
	}
}

void handleBlockedProcesses() {
}

void tryScheduleProcess() {
	Queue **activeSet = getActiveSet();
	int i;
	for (i = 0; i < QUEUE_RUN_COUNT; i++) {
		if (!queue_empty(activeSet[i])) {
			PCB *pcb = &global->shared->os.ptable[queue_pop(activeSet[i])];
			scheduleProcess(pcb);
			break;
		}
	}
}

void scheduleProcess(PCB *pcb) {
	global->shared->running = pcb;
	sendMessage(global->message, getChildQueue(), pcb->actualPID, "testing", false);
	logger("[DISPATCHED] Priority: %d, PID: %d", pcb->priority, pcb->localPID);
}

void cleanupResources(bool forced) {
	releaseSharedMemory();
	releaseMessageQueues();
}

void handleSignal(int signal) {
	printf("TEST1\n");
	kill(0, SIGUSR1);
	while (waitpid(-1, NULL, WNOHANG) >= 0);
	printf("TEST2\n");
	cleanupResources(true);
	exit(EXIT_SUCCESS);
}

int findAvailableLocalPID() {
	unsigned int i = 1, pos = 1;
	
	while ((i & global->vec) && pos <= PROCESSES_CONCURRENT_MAX) {
		i <<= 1;
		pos++;
	}
	
	return pos <= PROCESSES_CONCURRENT_MAX ? pos : -1;
}

PCB *getPCB(unsigned int localPID) {
	return &global->shared->os.ptable[localPID];
}

bool isProcessRunning() {
	return global->shared->running != NULL;
}

void initializeQueues() {
	int i;
	for (i = 0; i < QUEUE_RUN_COUNT; i++) {
		global->qset1[i] = queue_create(QUEUE_RUN_SIZE);
		global->qset2[i] = queue_create(QUEUE_RUN_SIZE);
	}
	
	global->active = global->qset1;
	global->expired = global->qset2;
	global->blocked = queue_create(QUEUE_BLOCK_SIZE);
}

void swapRunSets() {
	Queue **temp = global->active;
	global->active = global->expired;
	global->expired = temp;
}

Queue **getActiveSet() {
	return global->active;
}

void setActiveSet(Queue **set) {
	global->active = set;
}

Queue **getExpiredSet() {
	return global->expired;
}

void setExpiredSet(Queue **set) {
	global->expired = set;
}
