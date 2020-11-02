#ifndef HELPER_H
#define HELPER_H

#include "shared.h"

#define DEBUG true

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) < (y)) ? (x) : (y))

#define BIT_SET(a, b) ((a) |= (1ULL << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1ULL << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1ULL << (b)))
#define BIT_CHECK(a, b) (!!((a) & (1ULL << (b))))

int getLineCount(char*);
int getQueueQuantum(int, int);
int getUserQuantum(int, int);
void here(int);
void printBits(unsigned long int, int);

#endif
