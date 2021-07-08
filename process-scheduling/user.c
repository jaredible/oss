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
#include "user.h"

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
	srand(time(NULL) ^ (getpid() << 16));
	
	/* Infinite loop until we've terminated */
	while (true) {
		/* Check if we've received a message to simulate running */
		receiveMessage(&global->message, getChildQueue(), global->pcb->actualPID, true);
		
		/* Simulate running */
		if (shouldTerminate(global->pcb)) simulateProcessTerminated();
		else if (shouldExpire(global->pcb)) simulateProcessExpired();
		else simulateProcessBlocked();
	}
}

void simulateProcessTerminated() {
	/* Send a message to OSS saying we're terminating */
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "TERMINATED", true);
	
	/* Compute a random percentage from 1 to 99 */
	int rp = (rand() % 99) + 1;
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%d", rp);
	
	/* Send a message to OSS saying the percent of our user-quantum we've used */
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, buf, true);
	
	exit(EXIT_STATUS_OFFSET + global->pcb->localPID);
}

void simulateProcessExpired() {
	/* Send a message to OSS saying we've used our entire user-quantum */
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "EXPIRED", true);
}

void simulateProcessBlocked() {
	/* Send a message to OSS saying we're getting blocked */
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "BLOCKED", false);
	
	/* Compute a random percentage from 1 to 99 */
	int rp = (rand() % 99) + 1;
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%d", rp);
	
	/* Send a message to OSS saying the percent of our user-quantum we've used */
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, buf, true);
	
	/* Set a time in the future when this user process will be unblocked */
	Time unblock = { .sec = global->shared->system.sec, .ns = global->shared->system.ns };
	int addsec = rand() % (3 + 1);
	int addns = rand() % (1000 + 1);
	addTime(&unblock, addsec * 1e9 + addns);
	addTime(&global->pcb->block, addsec * 1e9 + addns);
	
	/* Busy-wait until the unblock time is reached */
	while (!(global->shared->system.sec >= unblock.sec && global->shared->system.ns >= unblock.ns));
	
	/* Send a message to OSS saying we're not unblocked */
	sendMessage(&global->message, getParentQueue(), global->pcb->actualPID, "UNBLOCKED", false);
}

bool shouldTerminate(PCB *pcb) {
	return rand() % 100 < (CHANCE_PROCESS_TERMINATES * (pcb->priority == 0 ? 2 : 1));
}

bool shouldExpire(PCB *pcb) {
	return rand() % 100 < CHANCE_PROCESS_EXPIRES;
}
