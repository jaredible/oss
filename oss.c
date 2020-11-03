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
#include <time.h>
#include <unistd.h>

#include "helper.h"
#include "queue.h"
#include "shared.h"

#define MAX_TIME_BETWEEN_NEW_PROCS_NS 150000
#define MAX_TIME_BETWEEN_NEW_PROCS_SEC 1

typedef unsigned long int bv_t;

typedef struct {
	unsigned int quantum;
	bool outputDigest;
	unsigned int timeout;
} Options;

typedef struct {
	Options options;
	Shared *shared;
	Queue *qset1[QUEUE_SET_COUNT];
	Queue *qset2[QUEUE_SET_COUNT];
	Queue **active;
	Queue **expired;
	Queue *blocked;
	Message *message;
	PCB *running;
	bv_t vector;
	Time nextSpawnAttempt;
	unsigned int spawnedProcessCount;
	unsigned int exitedProcessCount;
} Global;

void initializeProgram(int, char**);
void simulateOS();
bool canSpawnProcess();
void trySpawnProcess();
void spawnProcess();
void initializePCB(PCB*, unsigned int, pid_t);
void releasePCB(PCB*);
void handleProcessScheduling();
void handleRunningProcess();
void handleBlockedProcesses();
void tryScheduleProcess();
void trySwapQueueSets();
void scheduleProcess(PCB*);
void cleanupResources(bool);
void handleSignal(int);

void onProcessCreated(PCB*);
void onProcessScheduled(PCB*);
void onProcessTerminated(PCB*);
void onProcessExpired(PCB*);
void onProcessBlocked(PCB*);
void onProcessUnblocked(PCB*);
void onProcessExited(PCB*);

int findAvailableLocalPID();
PCB *getPCB(unsigned int);
bool isProcessRunning();
bool isProcessRealtime(PCB*);

void initializeQueues();
void swapRunSets();
Queue **getActiveSet();
void setActiveSet(Queue**);
Queue **getExpiredSet();
void setExpiredSet(Queue**);
Queue *getBlockedQueue();

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
	
//	timer(timeout);
	
	allocateSharedMemory(true);
	allocateMessageQueues(true);
	
	global->shared = getSharedMemory();
	global->shared->quantum = quantum;
}

void simulateOS() {
	global->message = (Message*) malloc(sizeof(Message));
	
	initializeQueues();
	
	//global->shared->system.sec = 0;
	//global->shared->system.ns = 0;
	//global->nextSpawnAttempt.sec = 0;
	//global->nextSpawnAttempt.ns = 0;
	
	srand(0);//time(NULL));
	
//	Queue *queue = queue_create(3);
//	queue_push(queue, 69);
//	queue_push(queue, 420);
//	queue_push(queue, 666);
//	queue_display(queue);
//	printf("Queue pop: %d\n", queue_pop(queue));
//	queue_display(queue);
//	queue_push(queue, 999);
//	printf("Queue pop: %d\n", queue_pop(queue));
//	queue_display(queue);
//	queue_push(queue, 1000);
//	printf("Queue pop: %d\n", queue_pop(queue));
//	queue_display(queue);
//	return;
//	printf("%d\n", getUserQuantum(10000, 0));
//	printf("%d\n", getUserQuantum(10000, 1));
//	printf("%d\n", getUserQuantum(10000, 2));
//	printf("%d\n", getUserQuantum(10000, 3));
//	printf("\n");
//	printf("%d\n", getQueueQuantum(10000, 0));
//	printf("%d\n", getQueueQuantum(10000, 1));
//	printf("%d\n", getQueueQuantum(10000, 2));
//	printf("%d\n", getQueueQuantum(10000, 3));
//	return;
	
//	while (true) {
//		addTime2(&global->shared->system, 1, 1e4);
//		printf("[%lu:%lu]\n", global->shared->system.sec, global->shared->system.ns);
//		if (global->shared->system.sec >= 0 && global->shared->system.ns >= 1e6) break;
//	}
//	return;
	
	while (true) {
		addTime(&global->shared->system, 0, 1e4);
		
		handleProcessScheduling();
		
		int status;
		pid_t pid = waitpid(-1, &status, WNOHANG);
		if (pid > 0) {
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) > 20) {
					int localPID = WEXITSTATUS(status) - 20;
					PCB *pcb = getPCB(localPID);
					onProcessExited(pcb);
				}
			}
		}
		
		if (global->exitedProcessCount == PROCESSES_TOTAL_MAX) break;
	}
}

bool canSpawnProcess() {
	Time *system = getSystemTime();
	Time *next = &global->nextSpawnAttempt;
	return global->spawnedProcessCount < PROCESSES_TOTAL_MAX && system->sec >= next->sec && system->ns >= next->ns && global->spawnedProcessCount < PROCESSES_TOTAL_MAX;
}

void trySpawnProcess() {
	if (canSpawnProcess()) {
		spawnProcess();
	}
}

void spawnProcess() {
	int localPID = findAvailableLocalPID();
	if (localPID > -1) {
		BIT_SET(global->vector, localPID - 1);
		
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
		onProcessCreated(pcb);
		
		queue_push(getActiveSet()[pcb->priority], localPID);
		logger("[CREATED] Priority: %d, PID: %d", pcb->priority, localPID);
		
		global->nextSpawnAttempt.sec = getSystemTime()->sec;
		global->nextSpawnAttempt.ns = getSystemTime()->ns;
		int addsec = abs(rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_SEC + 1);
		int addns = abs(rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_NS + 1);
		addTime(&global->nextSpawnAttempt, addsec, addns);
	}
}

void initializePCB(PCB *pcb, unsigned int localPID, pid_t actualPID) {
	pcb->priority = rand() % 2;
	pcb->localPID = localPID;
	pcb->actualPID = actualPID;
}

void releasePCB(PCB *pcb) {
	BIT_CLEAR(global->vector, pcb->localPID - 1);
	memset(&global->shared->ptable[pcb->localPID], 0, sizeof(PCB));
//	printBits(global->vector, PROCESSES_CONCURRENT_MAX);
}

void handleProcessScheduling() {
	trySpawnProcess();
	handleRunningProcess();
	trySwapQueueSets();
	handleBlockedProcesses();
	tryScheduleProcess();
}

void handleRunningProcess() {
	if (isProcessRunning()) {
		PCB *pcb = global->running;
		
		receiveMessage(global->message, getParentQueue(), pcb->actualPID, true);
		
		if (strcmp(global->message->text, "TERMINATED") == 0) onProcessTerminated(pcb);
		else if (strcmp(global->message->text, "EXPIRED") == 0) onProcessExpired(pcb);
		else if (strcmp(global->message->text, "BLOCKED") == 0) onProcessBlocked(pcb);
	}
}

void handleBlockedProcesses() {
	Queue *blocked = getBlockedQueue();
	if (!queue_empty(blocked)) {
//		queue_display(getBlockedQueue());
		int i;
		for (i = 0; i < blocked->size; i++) {
			PCB *pcb = getPCB(queue_pop(blocked));
//			if (msgrcv(getParentQueue(), global->message, sizeof(Message), pcb->actualPID, IPC_NOWAIT) > -1) {
//				printf("HERE %d %s\n", pcb->localPID, global->message->text);
//				if (strcmp(global->message, "UNBLOCKED") == 0) printf("HEREHERE\n");
//			}
			if (msgrcv(getParentQueue(), global->message, sizeof(Message), pcb->actualPID, IPC_NOWAIT) > -1 && strcmp(global->message->text, "UNBLOCKED") == 0) {
				queue_push(getActiveSet()[pcb->priority], pcb->localPID);
			} else {
				queue_push(getBlockedQueue(), pcb->localPID);
			}
		}
	}
}

void tryScheduleProcess() {
	if (isProcessRunning()) return;
	
	int i;
	for (i = 0; i < QUEUE_SET_COUNT; i++) {
		if (!queue_empty(getActiveSet()[i])) {
			int localPID = queue_pop(getActiveSet()[i]);
			PCB *pcb = &global->shared->ptable[localPID];
			scheduleProcess(pcb);
			break;
		}
	}
}

void trySwapQueueSets() {
	/* Don't continue if there's a process running */
	if (isProcessRunning()) return;
	
	/* Don't continue if there's a process in the active runqueues */
	int i;
	for (i = 0; i < QUEUE_SET_COUNT; i++) {
		if (!queue_empty(getActiveSet()[i])) return;
	}
	
	/* Don't continue if there's no process in the expired runqueues */
	bool flag = false;
	for (i = 0; i < QUEUE_SET_COUNT; i++) {
		if (!queue_empty(getExpiredSet()[i])) flag = true;
	}
	if (!flag) return;
	
//	printf("\nSwapping queues\n");
//	printf("Before swap\n");
//	for (i = 0; i < QUEUE_SET_COUNT; i++) {
//		printf("Active[%d] ", i); queue_display(getActiveSet()[i]);
//		printf("Expired[%d] ", i); queue_display(getExpiredSet()[i]);
//	}
	
	/* If there's no process running, no processes in the active queues, and a process in the expired queues, swap active and expired runqueues */
	swapRunSets();
	
//	printf("After swap\n");
//	for (i = 0; i < QUEUE_SET_COUNT; i++) {
//		printf("Active[%d] ", i); queue_display(getActiveSet()[i]);
//		printf("Expired[%d] ", i); queue_display(getExpiredSet()[i]);
//	}
//	printf("\n");
}

void scheduleProcess(PCB *pcb) {
	global->running = pcb;
	sendMessage(global->message, getChildQueue(), pcb->actualPID, "", false);
//	logger("[DISPATCHED] Priority: %d, PID: %d", pcb->priority, pcb->localPID);
	onProcessScheduled(pcb);
}

void cleanupResources(bool forced) {
	releaseSharedMemory();
	releaseMessageQueues();
}

void handleSignal(int signal) {
	printf("TEST1\n");
//	kill(0, SIGUSR1);
//	while(wait(NULL) > 0);//while (waitpid(-1, NULL, WNOHANG) >= 0);
	printf("TEST2\n");
	cleanupResources(true);
	exit(EXIT_SUCCESS);
}

void onProcessCreated(PCB *pcb) {
	global->spawnedProcessCount++;
}

void onProcessScheduled(PCB *pcb) {
}

void onProcessTerminated(PCB *pcb) {
	receiveMessage(global->message, getParentQueue(), pcb->actualPID, true);
	logger("[TERMINATED] PID: %d", pcb->localPID);
	global->running = NULL;
}

void onProcessExpired(PCB *pcb) {
	int previousPriority = pcb->priority;
	int nextPriority = pcb->priority;
	
	int time = 500;
	addTime2(&getPCB(pcb->localPID)->cpu, 0, time);
	addTime2(&getPCB(pcb->localPID)->queue, 0, time);
//	addTime2(&global->shared->system, 0, time);
	
	if (pcb->priority == 0) {
		queue_push(getActiveSet()[nextPriority], pcb->localPID);
	} else {
		if (pcb->priority < QUEUE_SET_COUNT - 1 && pcb->queue.sec >= 0 && pcb->queue.ns >= 1000) {
			nextPriority++;
			if (nextPriority > QUEUE_SET_COUNT - 1) nextPriority = QUEUE_SET_COUNT - 1;
			pcb->priority = nextPriority;
		}
		
		queue_push(getExpiredSet()[nextPriority], pcb->localPID);
	}
	
	global->running = NULL;
	
//	logger("[EXPIRED] PID: %d [%d->%d]", pcb->localPID, previousPriority, nextPriority);
}

void onProcessBlocked(PCB *pcb) {
	logger("[BLOCKED] PID: %d", pcb->localPID);
	queue_push(getBlockedQueue(), pcb->localPID);
	global->running = NULL;
}

void onProcessUnblocked(PCB *pcb) {
}

void onProcessExited(PCB *pcb) {
	global->exitedProcessCount++;
	releasePCB(pcb);
}

int findAvailableLocalPID() {
	unsigned int i = 1, pos = 1;
	
	while ((i & global->vector) && pos <= PROCESSES_CONCURRENT_MAX) {
		i <<= 1;
		pos++;
	}
	
	return pos <= PROCESSES_CONCURRENT_MAX ? pos : -1;
}

PCB *getPCB(unsigned int localPID) {
	return &global->shared->ptable[localPID];
}

bool isProcessRunning() {
	return global->running != NULL;
}

bool isProcessRealtime(PCB *pcb) {
	return pcb->priority == 0;
}

void initializeQueues() {
	int i;
	for (i = 0; i < QUEUE_SET_COUNT; i++) {
		global->qset1[i] = queue_create(QUEUE_SET_SIZE);
		global->qset2[i] = queue_create(QUEUE_SET_SIZE);
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

Queue *getBlockedQueue() {
	return global->blocked;
}
