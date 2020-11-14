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

#include "shared.h"
#include "time.h"
#include "user.h"

#define TERMINATION_CHANCE 50
#define REQUEST_CHANCE 60

Time *getClockTime();
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
	
	srand(time(NULL) ^ (pid << 16));
	
	while (true) {
		duration = subTime(*clock, arrival);
		
		if (duration.s >= 1 && clock->ns >= next.ns) {
			if (((rand() % 100) + 1) < TERMINATION_CHANCE) {
				sendTerminate(pid, spid);
				break;
			} else addTime(&next, rand() % (250000000 + 1));
		}
		
		if (((rand() % 100) + 1) < REQUEST_CHANCE) {
			rid = rand() % 20;
			response = sendRequest(rid, pid, spid);
			if (response == GRANT) {
				resources[rid]++;
				resourceCount++;
			} else if (response == DENY) {
				onWait(pid);
				resources[rid]++;
				resourceCount++;
			} else {
				crash("TEST");
			}
		} else if (resourceCount > 0) {
			int i;
			for (i = 0; i < 20; i++) {
				if (resources[i] > 0) {
					rid = i;
					break;
				}
			}
			
			response = sendRelease(rid, pid, spid);
			
			if (response == GRANT) {
				resources[rid]--;
				resourceCount--;
			}
		}
	}
	
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

int sendRequest(int rid, int pid, int spid) {
//	printf("[request] rid: %d, pid: %d, spid: %d\n", rid, pid, spid);
	Message msg = { .type = 1, .action = REQUEST, .rid = rid, .pid = spid, .sender = pid };
	if (msgsnd(msqid, &msg, sizeof(Message), 0) == -1) crash("msgsnd");
	if (msgrcv(msqid, &msg, sizeof(Message), pid, 0) == -1) crash("msgrcv");
	return msg.action;
}

int sendRelease(int rid, int pid, int spid) {
//	printf("[release] rid: %d, pid: %d, spid: %d\n", rid, pid, spid);
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
}
