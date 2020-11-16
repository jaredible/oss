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

#include "descriptor.h"
#include "oss.h"
#include "queue.h"
#include "shared.h"
#include "time.h"

void simulate();
Time *getSystemTime();
ResourceDescriptor *getResourceDescriptor();
void createMessageQueue();
void sendMessage(int, int);
void handleMessage(Message, Time*, ResourceDescriptor*, int*, Queue**);
bool deadlock(ResourceDescriptor*, int);
int getAvailablePID(int*, int);

static bool verbose = false;

static volatile bool quit = false;

static int spawnedProcessCount = 0;
static int exitedProcessCount = 0;

static int sysid;
static int rdid;
static int msqid;

static int spawnCount = 0;
static int activeCount = 0;
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
	clock->s = 0;
	clock->ns = 0;
	createMessageQueue();
	Message msg;
	Time next = { 0, 0 };
	ResourceDescriptor *rd = getResourceDescriptor();
	int *pids = (int*) malloc(18 * sizeof(int));
	Queue *wait[MAX_RESOURCES]; /* Each resource has a queue of spids */
	
	//int spid = queue_pop(queue[msg.rid]); /* Check if a process is waiting on the resource */
	//if (spid >= 0) sendMessage(spid, GRANT); /* Send the resource to the process */
	
	int i, j;
	
	for (i = 0; i < MAX_RESOURCES; i++)
		wait[i] = queue_create(MAX_PROCESSES);
	
	for (i = 0; i < 18; i++)
		pids[i] = -1;
	
	for (i = 0; i < MAX_PROCESSES; i++) {
		for (j = 0; j < MAX_RESOURCES; j++) {
			rd->requestMatrix[i][j] = 0;
			rd->allocationMatrix[i][j] = 0;
		}
	}
	for (i = 0; i < MAX_RESOURCES; i++) {
//		if (rand() % 5 == 0) {
//			rd->resourceVector[i] = 5;
//			rd->availableVector[i] = rd->resourceVector[i];
//			rd->sharedResourceVector[i] = 1;
//		} else {
			rd->resourceVector[i] = 1;//rand() % 10 + 1;
			rd->availableVector[i] = rd->resourceVector[i];
			rd->sharedResourceVector[i] = 0;
//		}
	}
	
	//printResourceDescriptor(*rd, MAX_RESOURCES, 18);
	
	srand(0);
	
	int n = 4;
	while (true) {
		if (!quit && spawnCount < n && clock->ns >= next.ns) {
			addTime(&next, 500000000);
			
			int spid = getAvailablePID(pids, 18);
			if (spid >= 0) {
				spawnCount++;
				activeCount++;
				pid_t pid = fork();
				pids[spid] = pid;
				if (pid == -1) crash("fork");
				else if (pid == 0) {
					char spidarg[3];
					char msqidarg[10];
					sprintf(spidarg, "%d", spid);
					sprintf(msqidarg, "%d", msqid);
					execl("./user", "user", spidarg, msqidarg, (char*) NULL);
					crash("execl");
				}
				//for (i = 0; i < MAX_RESOURCES; i++) {
				//	rd->maximumMatrix[spid][i] = rand() % (rd->resourceVector[i] + 1);
				//}
				//printf("\n");
				printf("./oss %d.%09ds p%-2d *--- %d\n", clock->s, clock->ns, spid, pid);
				//printVector("Maximum Vector", rd->maximumMatrix[spid], MAX_RESOURCES);
				//printf("\n");
			}
		} else if (msgrcv(msqid, &msg, sizeof(Message), 1, IPC_NOWAIT) > 0) {
			handleMessage(msg, clock, rd, pids, wait);
		}
		
		if (clock->ns == 0) {
//			bool deadlocked = deadlock(rd, 18);
		}
		
		if (exitCount == n) {
			printf("exitCount: %d, spawnCount: %d, quit: %s\n", exitCount, spawnCount, quit ? "true" : "false");
			break;
		}
		
		addTime(clock, 1000000);
	}
	
	for (i = 0; i < MAX_RESOURCES; i++)
		free(wait[i]);
	
	printf("cleanup\n");
	if (clock != NULL && shmdt(clock) == -1) crash("shmdt");
	if (sysid > 0 && shmctl(sysid, IPC_RMID, NULL) == -1) crash("shmctl");
	if (rd != NULL && shmdt(rd) == -1) crash("shmdt");
	if (rdid > 0 && shmctl(rdid, IPC_RMID, NULL) == -1) crash("shmctl");
	if (msqid > 0 && msgctl(msqid, IPC_RMID, NULL) == -1) crash("msgctl");
	
	cleanup();
	
	return EXIT_SUCCESS;
}

void sighandler(int signum) {
	if (quit) return;
	if (signum == SIGINT || signum == SIGALRM) quit = true;
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

void sendMessage(int pid, int action) { /* Rename to notifyUser? */
	Message msg = { .type = pid, .action = action, .pid = -1, .rid = -1, .sender = 1 };
	if (msgsnd(msqid, &msg, sizeof(Message), 0) == -1) crash("[oss] msgsnd");
}

void handleMessage(Message msg, Time *clock, ResourceDescriptor *rd, int *pids, Queue *queue[]) {
	if (msg.action == REQUEST) {
		//bool deadlocked = deadlock(rd, 18);
		if (rd->availableVector[msg.rid] > 0) {
			rd->availableVector[msg.rid] -= 1;
			rd->allocationMatrix[msg.pid][msg.rid] += 1;
			sendMessage(msg.sender, GRANT);
			printf("./oss %d.%09ds p%-2d -*-- r%-2d g\n", clock->s, clock->ns, msg.pid, msg.rid);
			//printMatrix("Allocation Matrix", rd->allocationMatrix, 18, 20);
		} else {
			queue_push(queue[msg.rid], msg.sender);
			// TODO: push into wait-queue?
			rd->requestMatrix[msg.pid][msg.rid] += 1; // TODO: do I need this?
			sendMessage(msg.sender, DENY);
			printf("./oss %d.%09ds p%-2d -*-- r%-2d d\n", clock->s, clock->ns, msg.pid, msg.rid);
			printf("p%d waiting for r%d\n", msg.pid, msg.rid);
			//queue_display(queue[msg.rid]);
			//sendMessage(msg.sender, -1); // TODO: testing
		}
	} else if (msg.action == RELEASE) {
		if (rd->allocationMatrix[msg.pid][msg.rid] > 0) {
			rd->availableVector[msg.rid] += 1;
//			int n = queue_pop(queue[msg.rid]);
//			printf("n: %d\n", n);
//			if (n >= 0) sendMessage(n, GRANT);
			rd->allocationMatrix[msg.pid][msg.rid] -= 1;
			sendMessage(msg.sender, GRANT);
			printf("./oss %d.%09ds p%-2d --*- r%-2d g\n", clock->s, clock->ns, msg.pid, msg.rid);
			int n = queue_pop(queue[msg.rid]);
			if (n >= 0) {
				rd->availableVector[msg.rid] -= 1;
				int a, i;
				for (i = 0; i < 18; i++)
					if (pids[i] == n) a = i;
				rd->allocationMatrix[a][msg.rid] += 1;
				printf("p%d acquired r%d\n", a, msg.rid);
				sendMessage(n, GRANT);
			}
		} else {
			sendMessage(msg.sender, DENY);
			fprintf(stderr, "BAD spid: %d, rid: %d\n", msg.pid, msg.rid);
		}
	} else if (msg.action == TERMINATE) {
		int i;
		for (i = 0; i < MAX_RESOURCES; i++) {
			printf("r%-2d ", i);
			queue_display(queue[i]);
			//rd->allocationMatrix[msg.pid][i] = 0;
			rd->requestMatrix[msg.pid][i] = 0;
		}
		while (rd->allocationMatrix[msg.pid][1] > 0) {
			int n = queue_pop(queue[1]);
			if (n >= 0) {
				int a, i;
				for (i = 0; i < 18; i++)
					if (pids[i] == n) a = i;
				rd->allocationMatrix[a][1] += 1;
				printf("p%d acquired r%d\n", a, 1);
				sendMessage(n, GRANT);
			}
			rd->allocationMatrix[msg.pid][1] -= 1;
		}
		// TODO: remove spid from wait-queue
		waitpid(msg.sender, NULL, 0);
		printf("./oss %d.%09ds p%-2d ---*\n", clock->s, clock->ns, msg.pid);
		exitCount++;
		activeCount--;
		pids[msg.pid] = -1;
	} else {
		//fprintf(stderr, "ERROR\n");
	}
}

int test(int req[], int avail[], int shared[], int held[]) {
	int i;
	for (i = 0; i < MAX_RESOURCES; i++) {
		if (shared[i] == 1 && req[i] > 0 && held[i] == 5) break;
		else if (req[i] > avail[i]) break;
	}
	return i == MAX_RESOURCES;
}

bool deadlock(ResourceDescriptor *rd, int processes) {
	int work[MAX_RESOURCES];
	bool finish[18];
	int deadlocked[18];
	int deadlockCount = 0;
	
	int i;
	int p;
	
	for (i = 0; i < MAX_RESOURCES; i++)
		work[i] = rd->availableVector[i];
	for (i = 0;i < processes; i++)
		finish[i] = false;
	
	for (p = 0; p < processes; p++) {
		if (finish[p]) continue;
		if (test(rd->requestMatrix[p], work, rd->sharedResourceVector, rd->allocationMatrix[p])) {
			finish[p] = true;
			for (i = 0; i < MAX_RESOURCES; i++)
				work[i] += rd->allocationMatrix[p][i];
			p = -1;
		}
	}
	for (p = 0; p < processes; p++)
		if (!finish[p]) deadlocked[deadlockCount++] = p;
	if (deadlockCount > 0) {
		printf("./oss Deadlock detected\n");
		for (i = 0; i < deadlockCount; i++)
			printf("./oss p%d deadlocked\n", deadlocked[i]);
		return true;
	}
	
	return false;
}

int getAvailablePID(int *pids, int size) {
	int i;
	for (i = 0; i < size; i++)
		if (pids[i] == -1) return i;
	return -1;
}
