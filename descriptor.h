#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

#include <stdbool.h>

#define MAX_PROCESSES 2
#define MAX_RESOURCES 2

typedef struct {
	int resource[MAX_RESOURCES];
	int available[MAX_RESOURCES];
	int maximum[MAX_PROCESSES][MAX_RESOURCES];
	int allocation[MAX_PROCESSES][MAX_RESOURCES];
	bool shared[MAX_RESOURCES];
} Descriptor;

void initDescriptor(Descriptor*);
void printVector(char*, int[], int);
void printMatrix(char*, int[][MAX_RESOURCES], int, int);
void printDescriptor(Descriptor, int, int);

#endif
