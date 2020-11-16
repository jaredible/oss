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
#include <time.h>
#include <wait.h>
#include <unistd.h>

#include "descriptor.h"
#include "oss.h"
#include "queue.h"
#include "shared.h"
#include "time.h"

#define OPTIONS "hv"
#define TIMEOUT 5
#define TOTAL_PROCESSES 2

void simulate();
Time *getSystemTime();
Descriptor *getDescriptor();
void createMessageQueue();
void sendMessage(int, int);
void handleMessage(Message, Time*, Descriptor*, int*, Queue**);
bool deadlock(Descriptor, int, int);
int getAvailablePID(int*);

static bool verbose = false;

static volatile bool quit = false;

static int sysid;
static int rdid;
static int msqid;

static int activeCount = 0;
static int spawnCount = 0;
static int exitCount = 0;

const int P = 2;
const int R = 2;

bool isSafe(int processes[], int available[], int maximum[][R], int allocation[][R]) {
	printVector("Processes", processes, P);
	printVector("Available", available, R);
	printMatrix("Maximum", maximum, P, R);
	printMatrix("Allocation", allocation, P, R);
	
	int i, j, k;
	
	bool finished[P];
	for (i = 0; i < P; i++)
		finished[i] = false;
	
	int need[P][R];
	for (i = 0; i < P; i++)
		for (j = 0; j < R; j++)
			need[i][j] = maximum[i][j] - allocation[i][j];
	
	int work[R];
	for (i = 0; i < R; i++)
		work[i] = available[i];
	
	int sequence[P];
	int count = 0;
	int p;
	
	while (count < P) {
		bool found = false;
		
		for (p = 0; p < P; p++) {
			if (finished[p]) continue;
			
			for (j = 0; j < R; j++)
				if (need[p][j] > work[j]) break;
			
			if (j != R) continue;
			
			for (k = 0; k < R; k++)
				work[k] += allocation[p][k];
			
			sequence[count++] = p;
			finished[p] = true;
			found = true;
		}
		
		if (!found) {
			printf("System NOT in safe state\n");
			return false;
		}
	}
	
	printf("System in safe state\n");
	for (i = 0; i < P - 1; i++)
		printf("%d ", sequence[i]);
	printf("%d\n", sequence[i]);
	
	return true;
}

void makeRequest(int processes[], int available[], int maximum[][R], int allocation[][R]) {
	printf("Before request\n");
	isSafe(processes, available, maximum, allocation);
	
	available[1] = 0;
	maximum[1][0] = 1; maximum[1][1] = 1;
	allocation[1][0] = 0; allocation[1][1] = 1;
	
	printf("\nAfter request\n");
	isSafe(processes, available, maximum, allocation);
}

void test() {
	int processes[] = { 0, 1 };
	
	int available[] = { 0, 1 };
	
	int maximum[P][R];
	maximum[0][0] = 1; maximum[0][1] = 1;// maximum[0][2] = 0; maximum[0][3] = 0;
	maximum[1][0] = 0; maximum[1][1] = 0;// maximum[1][2] = 0; maximum[1][3] = 0;
//	maximum[2][0] = 0; maximum[2][2] = 0; maximum[2][2] = 0; maximum[2][3] = 0;
//	maximum[3][0] = 0; maximum[3][2] = 0; maximum[3][2] = 0; maximum[3][3] = 0;
	
	int allocation[P][R];
	allocation[0][0] = 1; allocation[0][1] = 0;// allocation[0][2] = 0; allocation[0][3] = 0;
	allocation[1][0] = 0; allocation[1][1] = 0;// allocation[1][2] = 0; allocation[1][3] = 0;
//	allocation[2][0] = 0; allocation[2][1] = 0; allocation[2][2] = 0; allocation[2][3] = 0;
//	allocation[3][0] = 0; allocation[3][1] = 0; allocation[3][2] = 0; allocation[3][3] = 0;
	
//	isSafe(processes, available, maximum, allocation);
	makeRequest(processes, available, maximum, allocation);
}

int main(int argc, char **argv) {
	if (false) {
		test();
		return 0;
	}
	
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
	
	timer(TIMEOUT);
	
	Time *clock = getSystemTime();
	clock->s = 0;
	clock->ns = 0;
	createMessageQueue();
	Message msg;
	Time next = { .s = 0, .ns = 0 };
	Descriptor *descriptor = getDescriptor();
	int *pids = (int*) malloc(18 * sizeof(int));
	Queue *queue[MAX_RESOURCES]; /* Each resource has a queue of spids */
	
	int i;
	for (i = 0; i < MAX_RESOURCES; i++)
		queue[i] = queue_create(MAX_PROCESSES);
	for (i = 0; i < MAX_PROCESSES; i++)
		pids[i] = -1;
	
	initDescriptor(descriptor);
//	printDescriptor(*descriptor, MAX_PROCESSES, MAX_RESOURCES);
	
	srand(time(NULL));
	
	while (true) {
		if (activeCount < MAX_PROCESSES && spawnCount < TOTAL_PROCESSES && compareTime(*clock, next) > 0 && !quit) {
			addTime(&next, ((rand() % 500) + 1) * 1e6);
			printf("next: %d:%d\n", next.s, next.ns);
			
			int spid = getAvailablePID(pids);
			if (spid >= 0) {
				for (i = 0; i < MAX_RESOURCES; i++)
					descriptor->maximum[spid][i] = 1;//rand() % (descriptor->resource[i] + 1);
				
				pid_t pid = fork();
				if (pid == -1) crash("fork");
				else if (pid == 0) {
					char arg1[BUFFER_LENGTH];
					char arg2[BUFFER_LENGTH];
					sprintf(arg1, "%d", spid);
					sprintf(arg2, "%d", msqid);
					execl("./user", "user", arg1, arg2, (char*) NULL);
					crash("execl");
				}
				
				pids[spid] = pid;
				activeCount++;
				spawnCount++;
				
				slog("%d.%09ds p%-2d *-----", clock->s, clock->ns, spid);
			}
		} else if (msgrcv(msqid, &msg, sizeof(Message), 1, IPC_NOWAIT) > 0) {
			handleMessage(msg, clock, descriptor, pids, queue);
		}
		
		if (exitCount == TOTAL_PROCESSES) {
			printf("exitCount: %d, spawnCount: %d, quit: %s\n", exitCount, spawnCount, quit ? "true" : "false");
			break;
		}
		
		addTime(clock, 1 * 1000000);
	}
	
	for (i = 0; i < MAX_RESOURCES; i++)
		free(queue[i]);
	
//	if (clock != NULL && shmdt(clock) == -1) crash("shmdt");
//	if (sysid > 0 && shmctl(sysid, IPC_RMID, NULL) == -1) crash("shmctl");
//	if (rd != NULL && shmdt(rd) == -1) crash("shmdt");
//	if (rdid > 0 && shmctl(rdid, IPC_RMID, NULL) == -1) crash("shmctl");
//	if (msqid > 0 && msgctl(msqid, IPC_RMID, NULL) == -1) crash("msgctl");
	
	cleanup();
	
	return EXIT_SUCCESS;
}

void sighandler(int signum) {
	if (signum == SIGINT) {
		cleanup();
		exit(EXIT_FAILURE);
	}
	if (quit) return;
	if (signum == SIGALRM) quit = true;
}

void usage(int status) {
	if (status != EXIT_SUCCESS) fprintf(stderr, "Try '%s -h' for more information\n", getProgramName());
	else {
		printf("NAME\n");
		printf("       %s - OS resource management simulator\n", getProgramName());
		printf("USAGE\n");
		printf("       %s -h\n", getProgramName());
		printf("       %s [-v]\n", getProgramName());
		printf("DESCRIPTION\n");
		printf("       -h       : Prints usage information and exits\n");
		printf("       -v       : Prints program execution\n");
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
	if (sysid > 0 && shmctl(sysid, IPC_RMID, NULL) == -1) crash("shmctl");
	if (rdid > 0 && shmctl(rdid, IPC_RMID, NULL) == -1) crash("shmctl");
	if (msqid > 0 && msgctl(msqid, IPC_RMID, NULL) == -1) crash("msgctl");
}

Time *getSystemTime() {
	key_t key = ftok(".", CLOCK_KEY_ID);
	if (key < 0) crash("ftok");
	sysid = shmget(key, sizeof(Time), PERMS | IPC_EXCL | IPC_CREAT);
	if (sysid < 0) crash("shmget");
	Time *time = (Time*) shmat(sysid, NULL, 0);
	if (time < 0) crash("shmat");
	return time;
}

Descriptor *getDescriptor() {
	key_t key = ftok(".", DESCRIPTOR_KEY_ID);
	if (key < 0) crash("ftok");
	rdid = shmget(key, sizeof(Descriptor), PERMS | IPC_EXCL | IPC_CREAT);
	if (rdid < 0) crash("shmget");
	Descriptor *descriptor = (Descriptor*) shmat(rdid, NULL, 0);
	if (descriptor < 0) crash("shmat");
	return descriptor;
}

void createMessageQueue() {
	key_t key = ftok(".", QUEUE_KEY_ID);
	if (key < 0) crash("ftok");
	msqid = msgget(key, PERMS | IPC_EXCL | IPC_CREAT);
	if (msqid < 0) crash("msgget");
}

void sendMessage(int pid, int action) {
	Message msg = { .type = pid, .action = action, .pid = -1, .rid = -1, .sender = 1 };
	if (msgsnd(msqid, &msg, sizeof(Message), 0) == -1) crash("[oss] msgsnd");
}

void handleMessage(Message msg, Time *clock, Descriptor *descriptor, int *pids, Queue *queue[]) {
	int i;
	
	if (msg.action == REQUEST) {
		slog("%d.%09ds p%-2d -*---- r%-2d", clock->s, clock->ns, msg.pid, msg.rid);
		
		descriptor->available[msg.rid]--;
		descriptor->allocation[msg.pid][msg.rid]++;
		
		bool deadlocked = deadlock(*descriptor, MAX_PROCESSES, MAX_RESOURCES);
		printf("Deadlocked: %s\n", deadlocked ? "true" : "false");
		if (deadlocked) {
			queue_push(queue[msg.rid], msg.pid);
			sendMessage(msg.sender, DENY);
			slog("%d.%09ds p%-2d --*--- r%-2d", clock->s, clock->ns, msg.pid, msg.rid);
		} else {
			sendMessage(msg.sender, GRANT);
			slog("%d.%09ds p%-2d ---*-- r%-2d", clock->s, clock->ns, msg.pid, msg.rid);
		}
	} else if (msg.action == RELEASE) {
		if (descriptor->allocation[msg.pid][msg.rid] > 0) {
			descriptor->available[msg.rid]++;
			descriptor->allocation[msg.pid][msg.rid]--;
			
			sendMessage(msg.sender, GRANT);
			slog("%d.%09ds p%-2d ----*- r%-2d", clock->s, clock->ns, msg.pid, msg.rid);
			
			int spid = queue_pop(queue[msg.rid]);
			if (spid >= 0) {
				int pid = pids[spid];
				
				descriptor->available[msg.rid]--;
				descriptor->allocation[spid][msg.rid]++;
				
				sendMessage(pid, GRANT);
				slog("%d.%09ds p%-2d ---*-- r%-2d", clock->s, clock->ns, spid, msg.rid);
			}
		} else {
			fprintf(stderr, "p%d failed to release r%d", msg.pid, msg.rid);
			sendMessage(msg.sender, DENY);
		}
	} else if (msg.action == TERMINATE) {
		for (i = 0; i < MAX_RESOURCES; i++) {
			descriptor->maximum[msg.pid][i] = 0;
			
			while (descriptor->allocation[msg.pid][i] > 0) {
				descriptor->available[i]++;
				descriptor->allocation[msg.pid][i]--;
				
				int spid = queue_pop(queue[i]);
				if (spid >= 0) {
					int pid = pids[spid];
					
					descriptor->available[i]--;
					descriptor->allocation[spid][i]++;
					
					sendMessage(pid, GRANT);
					slog("%d.%09ds p%-2d ---*-- r%-2d", clock->s, clock->ns, spid, i);
				}
			}
		}
		
		waitpid(msg.sender, NULL, 0);
		slog("%d.%09ds p%-2d -----*", clock->s, clock->ns, msg.pid);
		
		pids[msg.pid] = -1;
		activeCount--;
		exitCount++;
	} else {
		slog("%d.%09ds Unknown message action: %d", clock->s, clock->ns, msg.action);
	}
}

bool deadlock(Descriptor descriptor, int processes, int resources) {
	printDescriptor(descriptor, processes, resources);
	
	int i, j;
	
	bool finished[processes];
	for (i = 0; i < processes; i++)
		finished[i] = false;
	
	int need[processes][resources];
	for (i = 0; i < processes; i++)
		for (j = 0; j < resources; j++)
			need[i][j] = descriptor.maximum[i][j] - descriptor.allocation[i][j];
	
//	printf("Need\n");
//	for (i = 0; i < processes; i++) {
//		for(j = 0; j < resources; j++)
//			printf("%d ", need[i][j]);
//		printf("\n");
//	}
	
	int work[resources];
	for (i = 0; i < resources; i++)
		work[i] = descriptor.available[i];
	
	int sequence[processes];
	int count = 0;
	
	for (i = 0; i < processes; i++) {
		if (finished[i]) continue;
		
		for (j = 0; j < resources; j++)
			if (need[i][j] > work[j]) break;
		
		if (j != resources) continue;
		
		for (j = 0; j < resources; j++)
			work[j] += descriptor.allocation[i][j];
		
		finished[i] = true;
//		printf("i: %d, j: %d, finished[i]: %s\n", i, j, finished[i] ? "true" : "false");
		i = -1;
	}
	
	for (i = 0; i < processes; i++)
		if (!finished[i]) sequence[count++] = i;
	
	if (count > 0) {
//		printf("Deadlocked\n");
		return true;
	}
	
//	printf("NOT deadlocked\n");
	return false;
}

int getAvailablePID(int *pids) {
	int i;
	for (i = 0; i < MAX_PROCESSES; i++)
		if (pids[i] == -1) return i;
	return -1;
}
