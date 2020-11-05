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
#define PROCESSES_CONCURRENT_MAX 2
#define PROCESSES_TOTAL_MAX 4
#define PATH_LOG "./output.log"

//#define QUANTUM_BASE_MIN 1e3
//#define QUANTUM_BASE_MAX 1e5
//#define QUANTUM_BASE_DEFAULT 1e4
//#define TIMEOUT_MIN 1
//#define TIMEOUT_MAX 10
//#define TIMEOUT_DEFAULT 3

#define QUEUE_SET_COUNT 4
#define QUEUE_SET_SIZE PROCESSES_CONCURRENT_MAX
#define QUEUE_BLOCK_SIZE PROCESSES_CONCURRENT_MAX

#define EXIT_STATUS_OFFSET 20

enum EventType { TERMINATED, EXPIRED, BLOCKED, UNBLOCKED };

typedef struct {
	long type;
	char text[BUFFER_LENGTH];
} Message;

/* Time data */
typedef struct {
	unsigned int sec; /* seconds */ // TODO: rename to "s"
	unsigned int ns; /* nanoseconds */
} Time;

/* Process Control Block (PCB) data */
typedef struct {
	pid_t actualPID; /* Actual PID */
	unsigned int localPID; /* Simulated PID */
	unsigned int priority; /* Queue index */
	Time arrival; /* Time created */
	Time exit; /* Time exited */
	Time burst; /* Time of last burst */
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
	unsigned int quantum; /* Base quantum for queues */ // TODO: rename?
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

void clearTime(Time*);
void copyTime(Time*, Time*);
void addTime(Time*, int, int);
Time subtractTime(Time*, Time*);
void showTime(Time*);

void sigact(int, void(int));

#endif
