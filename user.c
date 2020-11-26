/*
 * user.c November 21, 2020
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shared.h"

void init(int, char**);
void registerSignalHandlers();
void signalHandler(int);
void initIPC();
void crash(char*);

static char *programName;

/* IPC variables */
static int shmid = -1;
static int msqid = -1;
static System *system = NULL;
static Message message;

int main(int argc, char **argv) {
	init(argc, argv);
	
	int spid = atoi(argv[1]);
	
	srand(time(NULL) ^ (getpid() << 16));

	initIPC();

	Time start;
	Time end;
	bool canTerminate = false;
	bool hasResources = false;
	int i;
	
	/* Decision loop */
	while (true) {
		/* Wait until we get a message from OSS telling us it's our turn to "run" */
		msgrcv(msqid, &message, sizeof(Message), getpid(), 0);
		
		if (!canTerminate) {
			end.s = system->clock.s;
			end.ns = system->clock.ns;
			if (abs(end.ns - start.ns) >= 1000000000) canTerminate = true;
			else if (abs(end.s - start.s) >= 1) canTerminate = true;
		}
		
		/* Make a decision (i.e., request, release, or terminate) */
		int choice;
		do {
			choice = rand() % 3;
		} while ((choice == 1 && !hasResources) || (choice == 2 && !canTerminate));
		
		switch (choice) {
			case 0:
				message.type = 1;
				message.action = REQUEST;
				for (i = 0; i < RESOURCES_MAX; i++)
					message.request[i] = rand() % (system->ptable[spid].maximum[i] - system->ptable[spid].allocation[i] + 1);
				msgsnd(msqid, &message, sizeof(Message), 0);
				msgrcv(msqid, &message, sizeof(Message), getpid(), 0);
				if (message.acquired) hasResources = true;
				message.acquired = false;
				break;
			case 1:
				message.type = 1;
				message.action = RELEASE;
				msgsnd(msqid, &message, sizeof(Message), 0);
				break;
			case 2:
				message.type = 1;
				message.action = TERMINATE;
				msgsnd(msqid, &message, sizeof(Message), 0);
				exit(spid);
		}
	}
	
	return spid;
}

void init(int argc, char **argv) {
	programName = argv[0];

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}

void registerSignalHandlers() {
	signal(SIGUSR1, signalHandler);
	signal(SIGABRT, signalHandler);
}

void signalHandler(int sig) {
	printf("Terminating %d \n", getpid());
	exit(2);
}

void initIPC() {
	key_t key;

	if ((key = ftok(KEY_PATHNAME, KEY_ID_SYSTEM)) == -1) crash("ftok");
	if ((shmid = shmget(key, sizeof(System), 0)) == -1) crash("shmget");
	if ((system = (System*) shmat(shmid, NULL, 0)) == (void*) -1) crash("shmat");

	if ((key = ftok(KEY_PATHNAME, KEY_ID_MESSAGE_QUEUE)) == -1) crash("ftok");
	if ((msqid = msgget(key, 0)) == -1) crash("msgget");
}

void crash(char *msg) {
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%s: %s", programName, msg);
	perror(buf);
	
	exit(EXIT_FAILURE);
}
