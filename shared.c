#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>

#include "shared.h"

static char *programName = NULL;

static key_t shmkey;
static int shmid;
static Shared *shmptr = NULL;

void init(int argc, char **argv) {
	programName = basename(argv[0]);
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

void allocateSharedMemory() {
	if ((shmkey = ftok(KEY_PATHNAME, KEY_PROJID)) == -1) crash("ftok");
	if ((shmid = shmget(shmkey, sizeof(Shared), PERMS | IPC_EXCL | IPC_CREAT)) == -1) crash("shmget");
	else shmptr = (Shared*) shmat(shmid, NULL, 0);
}

void attachSharedMemory() {
	if ((shmkey = ftok(KEY_PATHNAME, KEY_PROJID)) == -1) crash("ftok");
	if ((shmid = shmget(shmkey, sizeof(Shared), PERMS)) == -1) crash("shmget");
	else shmptr = (Shared*) shmat(shmid, NULL, 0);
}

void releaseSharedMemory() {
	if (shmptr != NULL && shmdt(shmptr) == -1) crash("shmdt");
	if (shmid > 0 && shmctl(shmid, IPC_RMID, NULL) == -1) crash("shmctl");
}

Shared *getSharedMemory() {
	return shmptr;
}

void cleanup() {
	releaseSharedMemory();
}
