#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "shared.h"

typedef struct {
	Shared *shared;
	PCB *pcb;
	Message message;
} Global;

void initializeProgram(int, char**);
void simulateUser();

void simulateProcessTerminated();
void simulateProcessExpired();
void simulateProcessBlocked();

bool shouldTerminate();
bool shouldExpire();

void handleSignal(int signal);

static Global *global = NULL;

int main(int argc, char **argv) {
	global = (Global*) malloc(sizeof(Global));
	initializeProgram(argc, argv);
	simulateUser();
	return EXIT_SUCCESS;
}

void initializeProgram(int argc, char **argv) {
	init(argc, argv);
	
	sigact(SIGTERM, handleSignal);
	sigact(SIGUSR1, handleSignal);
	
	int localPID;
	
	if (argc < 2) {
		error("no argument supplied for local PID");
		exit(EXIT_FAILURE);
	} else localPID = atoi(argv[1]);
	
	allocateSharedMemory(false);
	allocateMessageQueues(false);
	
	global->shared = getSharedMemory();
	global->pcb = &global->shared->ptable[localPID];
}

void simulateUser() {
	srand(global->pcb->localPID);//time(NULL) ^ (getpid() << 16));
	
	while (true) {
		receiveMessage(&global->message, getChildQueue(), global->pcb->actualPID, true);
		
		if (shouldTerminate()) simulateProcessTerminated();
		else if (shouldExpire()) simulateProcessExpired();
		else simulateProcessBlocked();
	}
}

void simulateProcessTerminated() {
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "TERMINATED", true);
	
	int rp = (rand() % 99) + 1;
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%d", rp);
	
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, buf, true);
	
	exit(EXIT_STATUS_OFFSET + global->pcb->localPID);
}

void simulateProcessExpired() {
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "EXPIRED", true);
}

void simulateProcessBlocked() {
	Time unblock = { .sec = global->shared->system.sec, .ns = global->shared->system.ns };
	int addsec = rand() % (3 + 1);
	int addns = rand() % (1000 + 1);
	addTime(&unblock, addsec, addns);
	addTime(&global->pcb->block, addsec, addns);
	
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "BLOCKED", false);
	
	int rp = (rand() % 99) + 1;
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%d", rp);
	
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, buf, true);
	
	while (!(global->shared->system.sec >= unblock.sec && global->shared->system.ns >= unblock.ns));
	
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "UNBLOCKED", false);
}

bool shouldTerminate() {
	return rand() % 100 < 50;
}

bool shouldExpire() {
	return rand() % 2 == 0;
}

void handleSignal(int signal) {
	printf("signal: %d\n", signal);
	exit(EXIT_FAILURE);
}
