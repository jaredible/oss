#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include "shared.h"

#define PERMS (S_IRUSR | S_IWUSR)

static char *programName = NULL;

static key_t shmkey;
static int shmid;
static Shared *shmptr = NULL;

static key_t pmsqkey;
static int pmsqid;

static key_t cmsqkey;
static int cmsqid;

void init(int argc, char **argv) {
	programName = argv[0];
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}

void error(char *fmt, ...) {
	char buf[BUFFER_LENGTH];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, BUFFER_LENGTH, fmt, args);
	va_end(args);
	
	fprintf(stderr, "%s: %s\n", programName, buf);
	
	cleanup();
}

void crash(char *msg) {
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%s: %s", programName, msg);
	perror(buf);
	
	cleanup();
	
	exit(EXIT_FAILURE);
}

char *getProgramName() {
	return programName;
}

void allocateSharedMemory(bool init) {
	if ((shmkey = ftok("./Makefile", 'a')) == -1) crash("ftok");
	if ((shmid = shmget(shmkey, sizeof(Shared), PERMS | (init ? (IPC_EXCL | IPC_CREAT) : 0))) == -1) crash("shmget");
	else shmptr = (Shared*) shmat(shmid, NULL, 0);
}

void releaseSharedMemory() {
	if (shmptr != NULL && shmdt(shmptr) == -1) crash("shmdt");
	if (shmid > 0 && shmctl(shmid, IPC_RMID, NULL) == -1) crash("shmctl");
}

Shared *getSharedMemory() {
	return shmptr;
}

void allocateMessageQueues(bool init) {
	if ((pmsqkey = ftok("./Makefile", 'b')) == -1) crash("ftok");
	if ((pmsqid = msgget(pmsqkey, PERMS | (init ? (IPC_EXCL | IPC_CREAT) : 0))) == -1) crash("msgget");
	
	if ((cmsqkey = ftok("./Makefile", 'c')) == -1) crash("ftok");
	if ((cmsqid = msgget(cmsqkey, PERMS | (init ? (IPC_EXCL | IPC_CREAT) : 0))) == -1) crash("msgget");
}

void releaseMessageQueues() {
	if (pmsqid > 0 && msgctl(pmsqid, IPC_RMID, NULL) == -1) crash("msgctl");
	if (cmsqid > 0 && msgctl(cmsqid, IPC_RMID, NULL) == -1) crash("msgctl");
}

void sendMessage(Message *message, int msqid, pid_t address, char *msg, bool wait) {
	message->type = address;
	strncpy(message->text, msg, BUFFER_LENGTH);
	if (msgsnd(msqid, message, sizeof(Message), wait ? 0 : IPC_NOWAIT) == -1) crash("msgsnd");
//	printf("Sent %s to %ld\n", message->text, message->type);
}

void receiveMessage(Message *message, int msqid, pid_t address, bool wait) {
	if (msgrcv(msqid, message, sizeof(Message), address, wait ? 0 : IPC_NOWAIT) == -1) crash("msgrcv");
//	printf("Received %s from %ld\n", message->text, message->type);
}

int getParentQueue() {
	return pmsqid;
}

int getChildQueue() {
	return cmsqid;
}

void logger(char *fmt, ...) {
	FILE *fp = fopen(PATH_LOG, "a+");
	if (fp == NULL) crash("fopen");
	
	char buf[BUFFER_LENGTH];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, BUFFER_LENGTH, fmt, args);
	va_end(args);
	
	fprintf(fp, buf); // TODO: fix
	printf("%s: [%010d:%010d] %s\n", basename(programName), shmptr->system.sec, shmptr->system.ns, buf);
	
	fclose(fp);
}

void cleanup() {
	releaseSharedMemory();
	releaseMessageQueues();
}

void clearTime(Time *time) {
	time->sec = 0;
	time->ns = 0;
}

void copyTime(Time *source, Time *target) {
	memcpy(target, source, sizeof(Time));
}

void addTime(Time *time, int sec, int ns) {
	time->sec += sec;
	time->ns += ns;
	while (time->ns >= 1e9) {
		time->ns -= 1e9;
		time->sec++;
	}
}

Time subtractTime(Time *minuend, Time *subtrahend) {
	Time difference = { .sec = minuend->sec - subtrahend->sec, .ns = minuend->ns - subtrahend->ns };
	if (difference.ns < 0) {
		difference.ns += 1e9;
		difference.sec--;
	}
	return difference;
}

void showTime(Time *time) {
	printf("%d:%d\n", time->sec, time->ns);
}
