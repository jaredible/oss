#ifndef OSS_H
#define OSS_H

#include <stdbool.h>

#define TOTAL_PROCS_MIN 1
#define TOTAL_PROCS_MAX 40

#define CONCURRENT_PROCS_MIN 1
#define CONCURRENT_PROCS_MAX 18

#define RESOURCES_MIN 1
#define RESOURCES_MAX 20

#define TIMEOUT_MIN 1
#define TIMEOUT_MAX 5

#define OPTIONS "hvn:s:t:"

#define MAX_PROCESSES 18
#define MAX_RESOURCES 20

typedef struct {
	int requestMatrix[MAX_PROCESSES][MAX_RESOURCES];
	int maximumMatrix[MAX_PROCESSES][MAX_RESOURCES];
	int allocationMatrix[MAX_PROCESSES][MAX_RESOURCES];
	int resourceVector[MAX_RESOURCES];
	int availableVector[MAX_RESOURCES];
	int sharedResourceVector[MAX_RESOURCES];
} ResourceDescriptor;

/* Program setup */
void sighandler(int);
void usage(int);
void timer(int);
void cleanup();

#endif
