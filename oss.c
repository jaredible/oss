/*
 * oss.c 11/9/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

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
	int processCountRealtime;
	int processCountNormal;
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

static volatile bool quit = false;

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
	
	bool ok = true;
	
	while (true) {
		int c = getopt(argc, argv, "h");
		if (c == -1) break;
		switch (c) {
			case 'h':
				usage(EXIT_SUCCESS);
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
	
	timer(TIMEOUT);
	
	allocateSharedMemory(true);
	allocateMessageQueues(true);
	
	global->shared = getSharedMemory();
	
	FILE *fp;
	if ((fp = fopen(PATH_LOG, "w")) == NULL) crash("fopen");
	if (fclose(fp) == EOF) crash("fclose");
}

void addTimeLong(Time *time, long amount) {
	long newns = time->ns + amount;
	while (newns >= 1e9) {
		newns -= 1e9;
		time->sec++;
	}
	time->ns = newns;
}

void avgTime(Time *time, int count) {
	long epoch = (time->sec * 1e9 + time->ns) / count;
	Time temp = { 0, 0 };
	addTimeLong(&temp, epoch);
	time->sec = temp.sec;
	time->ns = temp.ns;
}

void simulateOS() {
	global->message = (Message*) malloc(sizeof(Message));
	
	initializeQueues();
	
	setTime(&global->shared->system, 0);
	setTime(&global->nextSpawnAttempt, 0);
	
	srand(time(NULL));
	
	while (true) {
		addTime(&global->shared->system, 1e4);
		addTime(&global->idle, 1e4);
		
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
		
		if (!canSchedule()) break;
	}
	
	printf("SUMMARY\n");
	printf("\tReal-time processes: %d\n", global->processCountRealtime);
	printf("\tNormal processes: %d\n", global->processCountNormal);
	
	printf("TOTALS\n");
	printf("\tCPU:    %ld:%ld\n", global->totalCpu.sec, global->totalCpu.ns);
	printf("\tBlock:  %ld:%ld\n", global->totalBlock.sec, global->totalBlock.ns);
	printf("\tWait:   %ld:%ld\n", global->totalWait.sec, global->totalWait.ns);
	printf("\tSystem: %ld:%ld\n", global->shared->system.sec, global->shared->system.ns);
	printf("\tIdle:   %ld:%ld\n", global->idle.sec, global->idle.ns);
	
	avgTime(&global->totalCpu, global->exitedProcessCount);
	avgTime(&global->totalBlock, global->exitedProcessCount);
	avgTime(&global->totalWait, global->exitedProcessCount);
	avgTime(&global->shared->system, global->exitedProcessCount);
	avgTime(&global->idle, global->exitedProcessCount);
	
	printf("AVERAGES\n");
	printf("\tCPU:    %ld:%ld\n", global->totalCpu.sec, global->totalCpu.ns);
	printf("\tBlock:  %ld:%ld\n", global->totalBlock.sec, global->totalBlock.ns);
	printf("\tWait:   %ld:%ld\n", global->totalWait.sec, global->totalWait.ns);
	printf("\tSystem: %ld:%ld\n", global->shared->system.sec, global->shared->system.ns);
	printf("\tIdle:   %ld:%ld\n", global->idle.sec, global->idle.ns);
}

bool canSchedule() {
	return global->exitedProcessCount < global->spawnedProcessCount;
}

bool canSpawnProcess() {
	Time *system = &global->shared->system;
	Time *next = &global->nextSpawnAttempt;
	return !quit && global->spawnedProcessCount < PROCESSES_TOTAL_MAX && system->sec >= next->sec && system->ns >= next->ns && global->spawnedProcessCount < PROCESSES_TOTAL_MAX;
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
	pcb->priority = rand() % 100 < 5 ? 0 : 1;
	
	if (pcb->priority == 0) global->processCountRealtime++;
	else global->processCountNormal++;
	
	clearTime(&pcb->arrival);
	clearTime(&pcb->exit);
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
			addTime(&global->shared->system, 5e6);
			addTime(&global->idle, 5e6);
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

void handleSignal(int signal) {
	if (signal == SIGALRM) quit = true;
	else if (signal == SIGINT) {
		int i;
		for (i = 0; i < PROCESSES_CONCURRENT_MAX; i++) {
			PCB *pcb = &global->shared->ptable[i];
			if (pcb->localPID != 0) kill(pcb->actualPID, SIGTERM);
		}
		while (wait(NULL) > 0);
		cleanupResources(true);
		exit(EXIT_SUCCESS);
	}
}

void onProcessCreated(PCB *pcb) {
	global->spawnedProcessCount++;
	
	queue_push(getActiveSet()[pcb->priority], pcb->localPID);
	
	global->nextSpawnAttempt.sec = global->shared->system.sec;
	global->nextSpawnAttempt.ns = global->shared->system.ns;
	int addsec = abs(rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_SEC + 1);
	int addns = abs(rand() * rand()) % (MAX_TIME_BETWEEN_NEW_PROCS_NS + 1);
	addTime(&global->nextSpawnAttempt, addsec * 1e9 + addns);
	
	logger("%-6s PID: %2d, Priority: %d", "*-----", pcb->localPID, pcb->priority);
}

void onProcessScheduled(PCB *pcb) {
	logger("%-6s PID: %2d, Priority: %d", "-*----", pcb->localPID, pcb->priority);
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
	int cost = getUserQuantum(pcb->priority);
	int time = (int) ((double) cost * ((double) percent / (double) 100));
	
	addTime(&pcb->cpu, time);
	addTime(&pcb->queue, time);
	addTime(&global->shared->system, time);
	
	subTime(&pcb->wait, &pcb->cpu);
	subTime(&pcb->wait, &pcb->block);
	
	global->running = NULL;
	
	logger("%-6s PID: %2d, Priority: %d", "-----*", pcb->localPID, pcb->priority);
	printf("\nPROCESS TERMINATED\n");
	printf("\tActual PID: %d\n", pcb->actualPID);
	printf("\tLocal PID:  %d\n", pcb->localPID);
	printf("\tPriority:   %d\n", pcb->priority);
	printf("\tArrival:    %ld:%ld\n", pcb->arrival.sec, pcb->arrival.ns);
	printf("\tExit:       %ld:%ld\n", pcb->exit.sec, pcb->exit.ns);
	printf("\tCPU:        %ld:%ld\n", pcb->cpu.sec, pcb->cpu.ns);
	printf("\tWait:       %ld:%ld\n", pcb->wait.sec, pcb->wait.ns);
	printf("\tBlock:      %ld:%ld\n\n", pcb->block.sec, pcb->block.ns);
}

void onProcessExpired(PCB *pcb) {
	int previousPriority = pcb->priority;
	int nextPriority = pcb->priority;
	
	int percent = 100;
	int cost = getUserQuantum(pcb->priority);
	int time = (int) ((double) cost * ((double) percent / (double) 100));
	
	addTime(&pcb->cpu, time);
	addTime(&pcb->queue, time);
	addTime(&global->shared->system, time);
	
	if (pcb->priority == 0) { /* Process is real-time */
		queue_push(getActiveSet()[nextPriority], pcb->localPID);
		logger("%-6s PID: %2d, Priority: %d", "--*---", pcb->localPID, pcb->priority);
	} else { /* Process is normal */
		int n = getQueueQuantum(pcb->priority);
		if (n != -1 && pcb->queue.ns >= n) {
			nextPriority++;
			if (nextPriority > QUEUE_SET_COUNT - 1) nextPriority = QUEUE_SET_COUNT - 1;
			pcb->priority = nextPriority;
			clearTime(&pcb->queue);
			logger("%-6s PID: %2d, Priority: %d <- %d", "--*---", pcb->localPID, nextPriority, previousPriority);
		} else {
			logger("%-6s PID: %2d, Priority: %d", "--*---", pcb->localPID, pcb->priority);
		}
		
		queue_push(getExpiredSet()[nextPriority], pcb->localPID);
	}
	
	global->running = NULL;
}

void onProcessBlocked(PCB *pcb) {
	receiveMessage(global->message, getParentQueue(), pcb->actualPID, true);
	
	int percent = atoi(global->message->text);
	int cost = getUserQuantum(pcb->priority);
	int time = (int) ((double) cost * ((double) percent / (double) 100));
	
	addTime(&pcb->cpu, time);
	addTime(&pcb->queue, time);
	addTime(&global->shared->system, time);
	
	logger("%-6s PID: %2d, Priority: %d", "---*--", pcb->localPID, pcb->priority);
	queue_push(getBlockedQueue(), pcb->localPID);
	global->running = NULL;
}

void onProcessUnblocked(PCB *pcb) {
	queue_push(getActiveSet()[pcb->priority], pcb->localPID);
	logger("%-6s PID: %2d, Priority: %d", "----*-", pcb->localPID, pcb->priority);
}

void onProcessExited(PCB *pcb) {
	global->exitedProcessCount++;
	
	addTime(&global->totalCpu, pcb->cpu.sec * 1e9 + pcb->cpu.ns);
	addTime(&global->totalBlock, pcb->block.sec * 1e9 + pcb->block.ns);
	addTime(&global->totalWait, pcb->wait.sec * 1e9 + pcb->wait.ns);
	
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
