/*
 * oss.c 11/17/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#include <getopt.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <wait.h>
#include <unistd.h>

#include "oss.h"
#include "shared.h"
#include "time.h"

void simulate();
Time *getSystemTime();
ResourceDescriptor *getResourceDescriptor();
void createMessageQueue();
void sendMessage(int, int);
void handleMessage(Message, Time*, ResourceDescriptor*);
bool deadlock();

static bool verbose = false;
static int n = TOTAL_PROCS_MAX;
static int s = CONCURRENT_PROCS_MAX;
static int t = TIMEOUT_MAX;

static volatile bool stop = false;

//static int msgid = -1;
//static Message *msgptr = NULL;

//static int sysid = -1;
//static Time *sysptr = NULL;

//static int pcbid = -1;
//static PCB *pcbptr = NULL;

static int spawnedProcessCount = 0;
static int exitedProcessCount = 0;

static bitvector bv;

static int sysid;
static int rdid;
static int msqid;

static int spawnCount = 0;
static int exitCount = 0;

int main(int argc, char **argv) {
	init(argc, argv);
	
	signal(SIGINT, sighandler);
	signal(SIGALRM, sighandler);
	
	bool ok = true;
	
	while (true) {
		int c = getopt(argc, argv, OPTIONS);
		if (c == -1) break;
		switch (c) {
			case 'h':
				usage(EXIT_SUCCESS);
			case 'v':
				verbose = true;
				break;
			case 'n':
				if (!isdigit(*optarg) || (n = atoi(optarg)) < TOTAL_PROCS_MIN) {
					error("invalid total processes '%s'", optarg);
					ok = false;
				}
				break;
			case 's':
				if (!isdigit(*optarg) || (s = atoi(optarg)) < CONCURRENT_PROCS_MIN) {
					error("invalid concurrent processes '%s'", optarg);
					ok = false;
				}
				break;
			case 't':
				if (!isdigit(*optarg) || (t = atoi(optarg)) < TIMEOUT_MIN) {
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
		snprintf(buf, BUFFER_LENGTH, "found non-options(s): ");
		while (optind < argc) {
			strncat(buf, argv[optind++], BUFFER_LENGTH);
			if (optind < argc) strncat(buf, ", ", BUFFER_LENGTH);
		}
		error(buf);
		ok = false;
	}
	
	if (!ok) usage(EXIT_FAILURE);
	
//	timer(t);
	
	Time *clock = getSystemTime();
	createMessageQueue();
	Message msg;
	Time next = { 0, 0 };
	ResourceDescriptor *rd = getResourceDescriptor();
	
	int i, j;
	for (i = 0; i < MAX_PROCESSES; i++) {
		for (j = 0; j < MAX_RESOURCES; j++) {
			rd->requestMatrix[i][j] = 0;
			rd->allocationMatrix[i][j] = 0;
		}
	}
	for (i = 0; i < MAX_RESOURCES; i++) {
		rd->resourceVector[i] = rand() % 10 + 1;
		rd->allocationVector[i] = rd->resourceVector[i];
	}
	
	while (true) {
		if (spawnCount < 2 && clock->s >= next.ns && clock->ns >= next.ns) {
			spawnCount++;
			addTime(&next, rand() % ((unsigned int) (500 * 1e6)));
			
			pid_t pid = fork();
			if (pid == -1) crash("fork");
			else if (pid == 0) {
				char spidarg[3];
				char msqidarg[10];
				sprintf(spidarg, "%d", 18);
				sprintf(msqidarg, "%d", msqid);
				execl("./user", "user", spidarg, msqidarg, (char*) NULL);
				crash("execl");
			}
		}
		
		if (msgrcv(msqid, &msg, sizeof(Message), 1, IPC_NOWAIT) > 0) {
			handleMessage(msg, clock, rd);
		}
		
		if (clock->ns == 0) {
			bool deadlocked = deadlock();
			if (deadlocked) {
				//printf("HERE\n");
			}
		}
		
		if (exitCount == spawnCount) break;
		
		addTime(clock, 1 * 1e6);
	}
	
	if (clock != NULL && shmdt(clock) == -1) crash("shmdt");
	if (sysid > 0 && shmctl(sysid, IPC_RMID, NULL) == -1) crash("shmctl");
	if (msqid > 0 && msgctl(msqid, IPC_RMID, NULL) == -1) crash("msgctl");
	
	return 0;
	
	cleanup();
	
	return EXIT_SUCCESS;
}

void sighandler(int signum) {
	if (stop) return;
	if (signum == SIGINT || signum == SIGALRM) stop = true;
	exit(EXIT_FAILURE);
}

void usage(int status) {
	if (status != EXIT_SUCCESS) fprintf(stderr, "Try '%s -h' for more information\n", getProgramName());
	else {
		printf("NAME\n");
		printf("USAGE\n");
		printf("DESCRIPTION\n");
	}
	exit(status);
}

void timer(int seconds) {
	struct itimerval val;
	val.it_value.tv_sec = seconds;
	val.it_value.tv_usec = 0;
	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &val, NULL) == -1) crash("setitimer");
}

void cleanup() {
//	if (msgid > 0 && msgctl(msgid, IPC_RMID, NULL) == -1) crash("msgctl");
	
//	if (sysptr != NULL && shmdt(sysptr) == -1) crash("shmdt");
//	if (sysid > 0 && shmctl(sysid, IPC_RMID, NULL) == -1) crash("shmctl");
	
//	if (pcbptr != NULL && shmdt(pcbptr) == -1) crash("shmdt");
//	if (pcbid > 0 && shmctl(pcbid, IPC_RMID, NULL) == -1) crash("shmctl");
}

Time *getSystemTime() {
	key_t key = ftok(".", 0);
	if (key < 0) crash("ftok");
	sysid = shmget(key, sizeof(Time), PERMS | IPC_EXCL | IPC_CREAT);
	if (sysid < 0) crash("shmget");
	Time *time = (Time*) shmat(sysid, NULL, 0);
	if (time < 0) crash("shmat");
	return time;
}

ResourceDescriptor *getResourceDescriptor() {
	key_t key = ftok(".", 2);
	if (key < 0) crash("ftok");
	rdid = shmget(key, sizeof(ResourceDescriptor), PERMS | IPC_EXCL | IPC_CREAT);
	if (rdid < 0) crash("shmget");
	ResourceDescriptor *rd = (ResourceDescriptor*) shmat(rdid, NULL, 0);
	if (rd < 0) crash("shmat");
	return rd;
}

void createMessageQueue() {
	key_t key = ftok(".", 1);
	if (key < 0) crash("ftok");
	msqid = msgget(key, PERMS | IPC_EXCL | IPC_CREAT);
	if (msqid < 0) crash("msgget");
}

void sendMessage(int pid, int action) {
	Message msg = { .type = pid, .action = action, .pid = -1, .rid = -1, .sender = 1 };
	if (msgsnd(msqid, &msg, sizeof(Message), 0) == -1) crash("msgsnd");
}

void handleMessage(Message msg, Time *clock, ResourceDescriptor *rd) {
	if (msg.action == REQUEST) {
//		printf("./oss %d.%09ds p%d *-- r%d\n", clock->s, clock->ns, msg.pid, msg.rid);
		if (rd->allocationVector[msg.rid] > 0) {
			rd->allocationVector[msg.rid]--;
			rd->allocationMatrix[msg.pid][msg.rid]++;
			sendMessage(msg.sender, GRANT);
			printf("./oss %d.%09ds p%-2d *-- r%-2d g\n", clock->s, clock->ns, msg.pid, msg.rid);
		} else {
			rd->requestMatrix[msg.pid][msg.rid]++;
			sendMessage(msg.sender, DENY);
			printf("./oss %d.%09ds p%-2d *-- r%-2d d\n", clock->s, clock->ns, msg.pid, msg.rid);
		}
	} else if (msg.action == RELEASE) {
//		printf("./oss %d.%09ds p%d -*- r%d\n", clock->s, clock->ns, msg.pid, msg.rid);
		if (rd->allocationMatrix[msg.pid][msg.rid] > 0) {
			rd->allocationMatrix[msg.pid][msg.rid]--;
			sendMessage(msg.sender, GRANT);
			printf("./oss %d.%09ds p%-2d -*- r%-2d g\n", clock->s, clock->ns, msg.pid, msg.rid);
		} else {
			sendMessage(msg.sender, DENY);
			fprintf(stderr, "BAD\n");
		}
	} else if (msg.action == TERMINATE) {
		int i;
		for (i = 0; i < MAX_RESOURCES; i++) {
			rd->allocationMatrix[msg.pid][i] = 0;
			rd->requestMatrix[msg.pid][i] = 0;
		}
		waitpid(msg.sender, NULL, 0);
		printf("./oss %d.%09ds p%d --*\n", clock->s, clock->ns, msg.pid);
		exitCount++;
	} else {
		fprintf(stderr, "ERROR\n");
	}
}

bool deadlock() {
	return true;
}
