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

#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_LENGTH 1024
#define KEY_PATHNAME "."
#define KEY_PROJID 'o'
#define PERMS (S_IRUSR | S_IWUSR)
#define PROCESSES_COUNT 18
#define DEFAULT_QUANTUM_BASE 10000

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
	PCB ptable[PROCESSES_COUNT];
	Time system;
} OS;

/* Shared-memory data */
typedef struct {
	OS os;
	unsigned int quantum; /* Base quantum for queues */
} Shared;

void init(int, char**);
void error(char *fmt, ...);
void crash(char*);

void allocateSharedMemory();
void attachSharedMemory();
void releaseSharedMemory();
Shared *getSharedMemory();

void cleanup();

#endif
