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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define BIT_SET(a, b) ((a) |= (1ULL << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1ULL << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1ULL << (b)))
#define BIT_CHECK(a, b) (!!((a) & (1ULL << (b))))

typedef unsigned long int bitvector;

/* Program setup */
void usage(int);
void timer(int);
void sighandler(int);
void cleanup();

/* Simulation */
void simulate();
bool canSimulate();
void trySpawn();
bool canSpawn();
void spawn();
void initProcess();

/* Process events */
void onProcessCreated();
void onProcessRequested();
void onProcessReleased();
void onProcessTerminated();
void onProcessExited();

/* Util */
int findAvailablePID();
bool isSafe();
void displayVector();
void displayMatrix();

#endif
