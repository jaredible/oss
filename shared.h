/*
 * shared.h 11/02/20
 * Jared Diehl (jmddnb@umsystem.edu)
 * ---------------------------------
 * Provides data structures,
 * functions, and variables that are
 * used in multiple modules.
 */

#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <sys/types.h>

#define BUFFER_LENGTH 1024
#define PROCESSES_CONCURRENT_MAX 18
#define PROCESSES_TOTAL_MAX 100
#define PATH_LOG "./output.log"

#define QUANTUM_BASE_MIN 1e3
#define QUANTUM_BASE_MAX 1e5
#define QUANTUM_BASE_DEFAULT 1e4
#define DIGEST_OUTPUT_DEFAULT false
#define TIMEOUT_MIN 1
#define TIMEOUT_MAX 10
#define TIMEOUT_DEFAULT 3

#define QUEUE_RUN_COUNT 4
#define QUEUE_RUN_SIZE PROCESSES_CONCURRENT_MAX
#define QUEUE_BLOCK_SIZE PROCESSES_TOTAL_MAX

typedef struct {
	long type;
	char text[BUFFER_LENGTH];
} Message;

/* Time data */
typedef struct {
	unsigned long sec; /* seconds */
	unsigned long ns; /* nanoseconds */
} Time;

/* Process Control Block (PCB) data */
typedef struct {
	unsigned int priority; /* Queue index */
	unsigned int localPID; /* Simulated PID */
	pid_t actualPID; /* Actual PID */
	Time cpu; /* Time spent on the CPU */
	Time system; /* Time spent in the system */
	Time blocked; /* Time spent blocked */
	Time waiting; /* Time spent waiting */
} PCB;

/* OS data */
typedef struct {
	PCB ptable[PROCESSES_CONCURRENT_MAX];
	Time system;
	unsigned int quantum; /* Base quantum for queues */
} OS;

/* Shared-memory data */
typedef struct {
	OS os;
	PCB *running; /* Active process PCB */
} Shared;

void init(int, char**);
void error(char *fmt, ...);
void crash(char*);
char *getProgramName();

void allocateSharedMemory(bool);
void releaseSharedMemory();
Shared *getSharedMemory();

void allocateMessageQueues(bool);
void releaseMessageQueues();
void sendMessage(Message*, int, pid_t, char*, bool);
void receiveMessage(Message*, int, pid_t, bool);
int getParentQueue();
int getChildQueue();

void logger(char*, ...);
void cleanup();

Time *getSystemTime();

#endif
