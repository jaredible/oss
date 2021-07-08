/*
 * shared.h December 2, 2020
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#define system _system

#define BUFFER_LENGTH 4096

#define KEY_PATHNAME "."
#define KEY_ID_SYSTEM 0
#define KEY_ID_MESSAGE_QUEUE 1
#define KEY_ID_SEMAPHORE 2
#define PERMS (S_IRUSR | S_IWUSR)

#define PATH_LOG "output.log"
#define TIMEOUT 2
#define PROCESSES_MAX 18
#define PROCESSES_TOTAL 40

#define PAGE_COUNT 32
#define PROCESS_SIZE (PAGE_COUNT * 1000)
#define PAGE_SIZE 1000
#define MAX_PAGES (PROCESS_SIZE / PAGE_SIZE)

#define MEMORY_COUNT 256
#define MEMORY_SIZE (MEMORY_COUNT * 1000)
#define FRAME_SIZE PAGE_SIZE
#define MAX_FRAMES (MEMORY_SIZE / FRAME_SIZE)

enum SchemeType { RANDOM, WEIGHTED };

typedef unsigned int uint;

typedef struct {
	unsigned int s;
	unsigned int ns;
} Time;

typedef struct {
	long type;
	pid_t pid;
	int spid;
	bool terminate;
	unsigned int address;
	unsigned int page;
} Message;

typedef struct {
	uint frame;
	uint address: 8;
	uint protection;
	uint dirty;
	uint valid;
} PTE;

typedef struct {
	pid_t pid;
	int spid;
	PTE ptable[MAX_PAGES];
} PCB;

typedef struct {
	Time clock;
	PCB ptable[PROCESSES_MAX];
} System;

#endif
