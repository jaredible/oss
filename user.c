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
	Message *message;
} Global;

void initializeProgram(int, char**);
void simulateUser();

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
	
	int localPID;
	
	if (argc < 2) {
		error("no argument supplied for local PID");
		exit(EXIT_FAILURE);
	} else localPID = atoi(argv[1]);
	
	signal(SIGUSR1, handleSignal);
	
	allocateSharedMemory(false);
	allocateMessageQueues(false);
	
	global->shared = getSharedMemory();
	global->pcb = &global->shared->ptable[localPID];
}

void simulateUser() {
	global->message = (Message*) malloc(sizeof(Message));
	
	srand(0);//time(NULL) ^ (getpid() << 16));
	
	bool usingIO = false;
	
	while (true) {
		receiveMessage(global->message, getChildQueue(), global->pcb->actualPID, true);
		
		bool shouldTerminate = rand() % 100 < 10;
		bool useEntireQuantum = rand() % 2 == 1;
		
		if (shouldTerminate && !usingIO) {
			sendMessage(global->message, getParentQueue(), global->pcb->actualPID, "TERMINATED", true);
			char buf[BUFFER_LENGTH];
			snprintf(buf, BUFFER_LENGTH, "%d", (rand() % 99) + 1);
			sendMessage(global->message, getParentQueue(), global->pcb->actualPID, buf, true);
			exit(20 + global->pcb->localPID);
		}
		
		if (true | useEntireQuantum) {
			sendMessage(global->message, getParentQueue(), global->pcb->actualPID, "EXPIRED", true);
		} else if (!usingIO) {
		}
	}
}

void handleSignal(int signal) {
	printf("signal: %d\n", signal);
	exit(EXIT_SUCCESS);
}
