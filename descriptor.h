#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

#define MAX_PROCESSES 18
#define MAX_RESOURCES 20

typedef struct {
	int resource[MAX_RESOURCES];
	int available[MAX_RESOURCES];
	int maximum[MAX_PROCESSES][MAX_RESOURCES];
	int allocation[MAX_PROCESSES][MAX_RESOURCES];
	int shared[MAX_RESOURCES];
} Descriptor;

void initDescriptor(Descriptor*);
void printVector(char*, int[], int);
void printMatrix(char*, int[][MAX_RESOURCES], int, int);
void printDescriptor(Descriptor*, int, int);

#endif
