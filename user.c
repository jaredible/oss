/*
 * user.c 11/9/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

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

bool shouldTerminate(PCB*);
bool shouldExpire(PCB*);

static Global *global = NULL;

int main(int argc, char **argv) {
	global = (Global*) malloc(sizeof(Global));
	initializeProgram(argc, argv);
	simulateUser();
	return EXIT_SUCCESS;
}

void initializeProgram(int argc, char **argv) {
	init(argc, argv);
	
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
		
		if (shouldTerminate(global->pcb)) simulateProcessTerminated();
		else if (shouldExpire(global->pcb)) simulateProcessExpired();
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
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "BLOCKED", false);
	
	int rp = (rand() % 99) + 1;
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%d", rp);
	
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, buf, true);
	
	Time unblock = { .sec = global->shared->system.sec, .ns = global->shared->system.ns };
	int addsec = rand() % (3 + 1);
	int addns = rand() % (1000 + 1);
	addTime(&unblock, addsec * 1e9 + addns);
	addTime(&global->pcb->block, addsec * 1e9 + addns);
	
	while (!(global->shared->system.sec >= unblock.sec && global->shared->system.ns >= unblock.ns));
	
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "UNBLOCKED", false);
}

bool shouldTerminate(PCB *pcb) {
	return rand() % 100 < (pcb->priority == 0 ? 20 : 10);
}

bool shouldExpire(PCB *pcb) {
	return rand() % 100 < 80;
}
