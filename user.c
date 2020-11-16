/*
 * user.c 11/17/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "oss.h"
#include "shared.h"
#include "time.h"
#include "user.h"

#define TERMINATION_CHANCE 80
#define REQUEST_CHANCE 20

Time *getClockTime();
ResourceDescriptor *getResourceDescriptor();
int sendRequest(int, int, int);
int sendRelease(int, int, int);
void sendTerminate(int, int);
void onWait(int);

static int msqid;

int main(int argc, char **argv) {
	init(argc, argv);
	
	if (argc <= 2) error("missing argument(s)");
	
	Time *clock;
	Time arrival;
	Time duration;
	Time next = { 0, 0 };
	int pid;
	int spid;
	int rid;
	int response;
	int resources[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int resourceCount = 0;
	
	pid = getpid();
	spid = atoi(argv[1]);
	msqid = atoi(argv[2]);
	clock = getClockTime();
	arrival = *clock;
	
//	ResourceDescriptor *rd = getResourceDescriptor();
//	int j;
//	for (j = 0; j < MAX_RESOURCES; j++) {
//		//resources[j] += rd->maximumMatrix[spid][j];
//		//resourceCount += resources[j];
//	}
	
	srand(spid);//time(NULL) ^ (pid << 16));
//	int i;
	while (true) {
		duration = subTime(*clock, arrival);
		
		if (duration.s >= 1 && clock->ns >= next.ns) {
			if (((rand() % 100) + 1) < TERMINATION_CHANCE) {
				sendTerminate(pid, spid);
				break;
			} else addTime(&next, rand() % (250000000 + 1));
		}
		
		if (((rand() % 100) + 1) < REQUEST_CHANCE) {
			if (resources[rid] >= 1) continue;
			rid = 1;//rand() % 20;
//			fprintf(stderr, "[request] p%d r%d    ", spid, rid);
//			fflush(stderr);
//			for (i = 0; i < 20; i++)
//				printf("%-2d ", resources[i]);
//			printf("\n");
			response = sendRequest(rid, pid, spid);
			if (response == GRANT) {
				resources[rid] += 1;
				resourceCount += 1;
			} else if (response == DENY) {
				onWait(pid);
				resources[rid] += 1;
				resourceCount += 1;
			} else {
				printf("HERE\n");//crash("TEST");
			}
//			fprintf(stderr, "granted p%d r%d    ", spid, rid);
//			for (i = 0; i < 20; i++)
//				printf("%-2d ", resources[i]);
//			printf("\n");
		} else if (resourceCount > 0) {
			int i;
			for (i = 0; i < MAX_RESOURCES; i++) {
				if (resources[i] > 0) {
					rid = i;
					break;
				}
			}
			
//			fprintf(stderr, "\n[before release] p%d r%d\n", spid, rid);
//			for (i = 0; i < MAX_RESOURCES; i++)
//				fprintf(stderr, "%-2d ", resources[i]);
//			fprintf(stderr, "\n");
			
			response = sendRelease(rid, pid, spid);
			
			if (response == GRANT) {
				resources[rid] -= 1;
				resourceCount -= 1;
			}
			
//			fprintf(stderr, "[after release] p%d r%d\n", spid, rid);
//			for (i = 0; i < MAX_RESOURCES; i++)
//				fprintf(stderr, "%-2d ", resources[i]);
//			fprintf(stderr, "\n\n");
		}
	}
	
	printf("p%d exiting\n", spid);
	return EXIT_STATUS_OFFSET + spid;
}

Time *getClockTime() {
	key_t key = ftok(".", 0);
	if (key < 0) crash("ftok");
	int shmid = shmget(key, sizeof(Time), 0);
	if (shmid < 0) crash("shmget");
	Time *time = (Time*) shmat(shmid, NULL, 0);
	if (time < 0) crash("shmat");
	return time;
}

ResourceDescriptor *getResourceDescriptor() {
	key_t key = ftok(".", 2);
	if (key < 0) crash("ftok");
	int shmid = shmget(key, sizeof(ResourceDescriptor), 0);
	if (shmid < 0) crash("shmget");
	ResourceDescriptor *rd = (ResourceDescriptor*) shmat(shmid, NULL, 0);
	if (rd < 0) crash("shmat");
	return rd;
}

int sendRequest(int rid, int pid, int spid) {
	Message msg = { .type = 1, .action = REQUEST, .rid = rid, .pid = spid, .sender = pid };
	if (msgsnd(msqid, &msg, sizeof(Message), 0) == -1) crash("msgsnd");
	if (msgrcv(msqid, &msg, sizeof(Message), pid, 0) == -1) crash("msgrcv");
	return msg.action;
}

int sendRelease(int rid, int pid, int spid) {
	Message msg = { .type = 1, .action = RELEASE, .rid = rid, .pid = spid, .sender = pid };
	if (msgsnd(msqid, &msg, sizeof(Message), 0) == -1) crash("msgsnd");
	if (msgrcv(msqid, &msg, sizeof(Message), pid, 0) == -1) crash("msgsnd");
	return msg.action;
}

void sendTerminate(int pid, int spid) {
//	printf("[terminate] pid: %d, spid: %d\n", pid, spid);
	Message msg = { .type = 1, .action = TERMINATE, .rid = -1, .pid = spid, .sender = pid };
	if (msgsnd(msqid, &msg, sizeof(Message), 0) == -1) crash("msgsnd");
}

void onWait(int pid) {
	Message msg;
	if (msgrcv(msqid, &msg, sizeof(Message), pid, 0) == -1) crash("msgrcv");
	printf("p%d waked\n", pid);
}
