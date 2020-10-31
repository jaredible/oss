#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared.h"

typedef struct {
	Shared *shared;
	Message *message;
	PCB *pcb;
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
	global->pcb = &global->shared->os.ptable[localPID];
}

void simulateUser() {
	global->message = (Message*) malloc(sizeof(Message));
	int n = 10;
	while (true) {
		receiveMessage(global->message, getChildQueue(), getpid(), true);
		sendMessage(global->message, getParentQueue(), getpid(), n == 0 ? "TERMINATED" : "EXPIRED", 0);
		if (n == 0) exit(21);
		n--;
		//sleep(1);
	}
}

void handleSignal(int signal) {
	printf("signal: %d\n", signal);
	exit(EXIT_SUCCESS);
}
