/*
 * shared.h 11/9/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <sys/types.h>

#define BUFFER_LENGTH 1024
#define PROCESSES_CONCURRENT_MAX 18
#define PROCESSES_TOTAL_MAX 100
#define PATH_LOG "./output.log"

#define QUANTUM_BASE 1e2

#define QUEUE_SET_COUNT 4
#define QUEUE_SET_SIZE PROCESSES_CONCURRENT_MAX
#define QUEUE_BLOCK_SIZE PROCESSES_CONCURRENT_MAX

#define EXIT_STATUS_OFFSET 20

typedef struct {
	long type;
	char text[BUFFER_LENGTH];
} Message;

/* Time data */
typedef struct {
	unsigned int sec; /* seconds */
	unsigned int ns; /* nanoseconds */
} Time;

/* Process Control Block (PCB) data */
typedef struct {
	pid_t actualPID; /* Actual PID */
	unsigned int localPID; /* Simulated PID */
	unsigned int priority; /* Queue index */
	Time arrival; /* Time created */
	Time exit; /* Time exited */
	Time cpu; /* Time spent on CPU */
	Time queue; /* Time spent in queue */
	Time block; /* Time spent blocked */
	Time wait; /* Time spent waiting */
	Time system; /* Time spent in system */
} PCB;

/* Shared data between oss and users */
typedef struct {
	Time system;
	PCB ptable[PROCESSES_CONCURRENT_MAX];
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
int sendMessage(Message*, int, pid_t, char*, bool);
int receiveMessage(Message*, int, pid_t, bool);
int getParentQueue();
int getChildQueue();

void logger(char*, ...);
void cleanup();

void clearTime(Time*);
void copyTime(Time*, Time*);
void addTime(Time*, int, int);
Time subtractTime(Time*, Time*);
void showTime(Time*);

void sigact(int, void(int));
int getQueueQuantum(int);
int getUserQuantum(int);

#endif
