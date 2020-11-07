#include <getopt.h>
#include <limits.h> // TODO: remove
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
#include "oss.h"
#include "queue.h"
#include "shared.h"

#define MAX_TIME_BETWEEN_NEW_PROCS_NS 150000
#define MAX_TIME_BETWEEN_NEW_PROCS_SEC 1

typedef unsigned long int bv_t;

typedef struct {
	Shared *shared;
	Queue *qset1[QUEUE_SET_COUNT];
	Queue *qset2[QUEUE_SET_COUNT];
	Queue **active;
	Queue **expired;
	Queue *blocked;
	Message *message;
	PCB *running;
	bv_t vector;
	Time idle;
	Time nextSpawnAttempt;
	unsigned int spawnedProcessCount;
	unsigned int exitedProcessCount;
	Time totalCpu;
	Time totalBlock;
	Time totalWait;
} Global;

void initializeProgram(int, char**);
void simulateOS();
bool canSchedule();
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
	
	/* Register signal handlers */
	signal(SIGINT, handleSignal);
	signal(SIGALRM, handleSignal);
//	sigact(SIGINT, handleSignal);
//	sigact(SIGALRM, handleSignal);
	
	bool ok = true;
	int timeout = OPTIONS_TIMEOUT_DEFAULT;
	
	while (true) {
		int c = getopt(argc, argv, "hq:t:dv");
		if (c == -1) break;
		switch (c) {
			case 'h':
				usage(EXIT_SUCCESS);
			case 't':
				if (!isdigit(*optarg) || (timeout = atoi(optarg)) <= TIMEOUT_MIN) {
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
		snprintf(buf, BUFFER_LENGTH, "found non-option(s): ");
		while (optind < argc) {
			strncat(buf, argv[optind++], BUFFER_LENGTH);
			if (optind < argc) strncat(buf, ", ", BUFFER_LENGTH);
		}
		error(buf);
		ok = false;
	}
	
	if (!ok) usage(EXIT_FAILURE);
	
	timer(1);
	
	allocateSharedMemory(true);
	allocateMessageQueues(true);
	
	global->shared = getSharedMemory();
	global->shared->quantum = QUANTUM_BASE_DEFAULT;
	
	FILE *fp;
	if ((fp = fopen("output.log", "w")) == NULL) crash("fopen");
	if (fclose(fp) == EOF) crash("fclose");
}

void simulateOS() {
	global->message = (Message*) malloc(sizeof(Message));
	
	initializeQueues();
	
	global->shared->system.sec = 0;
	global->shared->system.ns = 0;
	global->nextSpawnAttempt.sec = 0;
	global->nextSpawnAttempt.ns = 0;
	
	srand(0);//time(NULL));
	
	while (canSchedule()) {
		addTime(&global->shared->system, 0, 1e4);
		addTime(&global->idle, 0, 1e4);
		
		handleProcessScheduling();
		
		int status;
		pid_t pid = waitpid(-1, &status, WNOHANG);
		if (pid > 0) {
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) > EXIT_STATUS_OFFSET) {
					int localPID = WEXITSTATUS(status) - EXIT_STATUS_OFFSET;
					PCB *pcb = getPCB(localPID);
					onProcessExited(pcb);
				}
			}
		}
	}
	
//	copyTime(&global->shared->system, &global->totalSystem);
//	printf("%i:%i\n", global->totalSystem.sec, global->totalSystem.ns);
	
	printf("TOTAL TIMES\n");
	printf("CPU: %d:%d\n", global->totalCpu.sec, global->totalCpu.ns);
	printf("Block: %d:%d\n", global->totalBlock.sec, global->totalBlock.ns);
	printf("Wait: %d:%d\n", global->totalWait.sec, global->totalWait.ns);
	printf("System: %d:%d\n", global->shared->system.sec, global->shared->system.ns);
	printf("Idle: %d:%d\n", global->idle.sec, global->idle.ns);
	
//	double epoch = (((double) (global->totalCpu.sec * 1e9 + global->totalCpu.ns)) / ((double) global->exitedProcessCount)) / ((double) 100);
//	printf("Throughput: %.2f\n", epoch); // TODO: correct?
	
//	printf("AVERAGE TIMES\n");
}

bool canSchedule() {
	return !(global->spawnedProcessCount == PROCESSES_TOTAL_MAX && global->exitedProcessCount == PROCESSES_TOTAL_MAX);
}

bool canSpawnProcess() {
	Time *system = &global->shared->system;
	Time *next = &global->nextSpawnAttempt;
	return global->spawnedProcessCount < PROCESSES_TOTAL_MAX && system->sec >= next->sec && system->ns >= next->ns && global->spawnedProcessCount < PROCESSES_TOTAL_MAX;
}

void trySpawnProcess() {
	if (canSpawnProcess()) spawnProcess();
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
	}
}

void initializePCB(PCB *pcb, unsigned int localPID, pid_t actualPID) {
	pcb->localPID = localPID;
	pcb->actualPID = actualPID;
	pcb->priority = 1;//rand() % 100 < 20 ? 0 : 1;
	
	clearTime(&pcb->arrival);
	clearTime(&pcb->exit);
	clearTime(&pcb->burst);
	clearTime(&pcb->cpu);
	clearTime(&pcb->queue);
	clearTime(&pcb->block);
	clearTime(&pcb->wait);
	clearTime(&pcb->system);
	
	copyTime(&global->shared->system, &pcb->arrival);
	copyTime(&global->shared->system, &pcb->system);
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
		if (!isProcessRunning()) {
			addTime(&global->shared->system, 0, 5000000);
			addTime(&global->idle, 0, 5000000);
		}
		
		int i;
		for (i = 0; i < blocked->size; i++) {
			PCB *pcb = getPCB(queue_pop(blocked));
			if (msgrcv(getParentQueue(), global->message, sizeof(Message), pcb->actualPID, IPC_NOWAIT) > -1 && strcmp(global->message->text, "UNBLOCKED") == 0) {
				onProcessUnblocked(pcb);
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
			//printf("\n");
			//queue_display(getActiveSet()[i]);
			int localPID = queue_pop(getActiveSet()[i]);
			PCB *pcb = &global->shared->ptable[localPID];
			scheduleProcess(pcb);
			//queue_display(getActiveSet()[i]);
			//printf("\n");
			//i--;
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
	
	swapRunSets();
}

void scheduleProcess(PCB *pcb) {
	global->running = pcb;
	sendMessage(global->message, getChildQueue(), pcb->actualPID, "", false);
	onProcessScheduled(pcb);
}

void cleanupResources(bool forced) {
	releaseSharedMemory();
	releaseMessageQueues();
}

void handleSignal(int signum) {
	int i;
	for (i = 0; i < 18; i++) {
		PCB *pcb = &global->shared->ptable[i];
		if (pcb->localPID != 0) {
			//printf("PID: %d\n", pcb->localPID);
			kill(pcb->actualPID, SIGTERM);
		}
	}
//	printf("Before killing children\n");
//	kill(0, signal == SIGALRM ? SIGUSR1 : SIGTERM);
	//pid_t pid;
//	while(wait(NULL) > 0);//while ((pid = waitpid(-1, NULL, WNOHANG)) > -1);
//	printf("After killing children\n");
	
	cleanupResources(true);
	exit(EXIT_SUCCESS);
}

void onProcessCreated(PCB *pcb) {
	global->spawnedProcessCount++;
	
	queue_push(getActiveSet()[pcb->priority], pcb->localPID);
	
	global->nextSpawnAttempt.sec = global->shared->system.sec;
	global->nextSpawnAttempt.ns = global->shared->system.ns;
	int addsec = abs(rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_SEC + 1);
	int addns = abs(rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_NS + 1);
	addTime(&global->nextSpawnAttempt, addsec, addns);
	
	logger("%-6s Priority: %d, PID: %d", "*-----", pcb->priority, pcb->localPID);
}

void onProcessScheduled(PCB *pcb) {
	logger("%-6s Priority: %d, PID: %d", "-*----", pcb->priority, pcb->localPID);
}

void addTimeLong(Time *time, long amount) {
	long newns = time->ns + amount;
	while (newns >= 1e9) {
		newns -= 1e9;
		time->sec++;
	}
	time->ns = newns;
}

void subTime(Time *a, Time *b) {
	long epoch1 = a->sec * 1e9 + a->ns;
	long epoch2 = b->sec * 1e9 + b->ns;
	
	long diff = abs(epoch1 - epoch2);
	
	Time temp = { 0, 0 };
	
	addTimeLong(&temp, diff);
	
	a->sec = temp.sec;
	a->ns = temp.ns;
}

void onProcessTerminated(PCB *pcb) {
	receiveMessage(global->message, getParentQueue(), pcb->actualPID, true);
	
	copyTime(&global->shared->system, &pcb->exit);
	
	int percent = atoi(global->message->text);
	int cost = -1;
	switch (pcb->priority) {
		case 0: cost = 10000; break;
		case 1: cost = 5000; break;
		case 2: cost = 2500; break;
		case 3: cost = 1250; break;
	}
	int time = (int) ((double) cost * ((double) percent / (double) 100));
//	printf("percent: %d, cost: %d, time: %d\n", percent, cost, time);
	
	addTime(&pcb->cpu, 0, time);
	addTime(&pcb->queue, 0, time);
	addTime(&global->shared->system, 0, time);
	
	subTime(&pcb->wait, &pcb->cpu);
	subTime(&pcb->wait, &pcb->block);
	
	/* Calculate process' total time waiting */
//	pcb->wait.sec = pcb->exit.sec - (pcb->arrival.sec + pcb->cpu.sec);
//	long epoch = pcb->arrival.ns + pcb->cpu.ns;
//	if (pcb->exit.ns < epoch) {
//		pcb->wait.sec--;
//		pcb->wait.ns += 1e9;
//		pcb->wait.ns += (pcb->exit.ns - epoch);
//	} else {
//		pcb->wait.ns = epoch;
//	}
	
	
	global->running = NULL;
	
	logger("%-6s Priority: %d, PID: %d", "-----*", pcb->priority, pcb->localPID);
	printf("CPU time: %d:%d\n", pcb->cpu.sec, pcb->cpu.ns);
	printf("Wait time: %d:%d\n", pcb->wait.sec, pcb->wait.ns);
	printf("Block time: %d:%d\n", pcb->block.sec, pcb->block.ns);
	printf("\n");
}

void onProcessExpired(PCB *pcb) {
	int previousPriority = pcb->priority;
	int nextPriority = pcb->priority;
	
	int percent = 100;
	int cost = -1;
	switch (pcb->priority) {
		case 0: cost = 10000; break;
		case 1: cost = 5000; break;
		case 2: cost = 2500; break;
		case 3: cost = 1250; break;
	}
	int time = (int) ((double) cost * ((double) percent / (double) 100));
//	printf("percent: %d, cost: %d, time: %d\n", percent, cost, time);
	
//	clearTime(&pcb->burst);
//	addTime(&pcb->burst, 0, time);
	addTime(&pcb->cpu, 0, time);
	addTime(&pcb->queue, 0, time);
	addTime(&global->shared->system, 0, time);
	
	if (pcb->priority == 0) {
		queue_push(getActiveSet()[nextPriority], pcb->localPID);
		logger("%-6s Priority: %d, PID: %d", "--*---", pcb->priority, pcb->localPID);
	} else {
		if (pcb->priority < QUEUE_SET_COUNT - 1 && pcb->queue.sec >= 0 && pcb->queue.ns >= 1000) {
			nextPriority++;
			if (nextPriority > QUEUE_SET_COUNT - 1) nextPriority = QUEUE_SET_COUNT - 1;
			pcb->priority = nextPriority;
			clearTime(&pcb->queue);
			logger("%-6s Priority: %d, PID: %d, Shifting: %d -> %d", "--*---", pcb->priority, pcb->localPID, previousPriority, nextPriority);
		} else {
			logger("%-6s Priority: %d, PID: %d", "--*---", pcb->priority, pcb->localPID);
		}
		
		queue_push(getExpiredSet()[nextPriority], pcb->localPID);
	}
	
	global->running = NULL;
}

void onProcessBlocked(PCB *pcb) {
	receiveMessage(global->message, getParentQueue(), pcb->actualPID, true);
	
	int percent = atoi(global->message->text);
	int cost = -1;
	switch (pcb->priority) {
		case 0: cost = 10000; break;
		case 1: cost = 5000; break;
		case 2: cost = 2500; break;
		case 3: cost = 1250; break;
	}
	int time = (int) ((double) cost * ((double) percent / (double) 100));
//	printf("percent: %d, cost: %d, time: %d\n", percent, cost, time);
	
	addTime(&pcb->cpu, 0, time);
	addTime(&pcb->queue, 0, time);
	addTime(&global->shared->system, 0, time);
	
	logger("%-6s Priority: %d, PID: %d", "---*--", pcb->priority, pcb->localPID);
	queue_push(getBlockedQueue(), pcb->localPID);
	global->running = NULL;
}

void onProcessUnblocked(PCB *pcb) {
	queue_push(getActiveSet()[pcb->priority], pcb->localPID);
	logger("%-6s Priority: %d, PID: %d", "----*-", pcb->priority, pcb->localPID);
}

void onProcessExited(PCB *pcb) {
	global->exitedProcessCount++;
	
	addTime(&global->totalCpu, pcb->cpu.sec, pcb->cpu.ns);
	addTime(&global->totalBlock, pcb->block.sec, pcb->block.ns);
	addTime(&global->totalWait, pcb->wait.sec, pcb->wait.ns);
	
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
