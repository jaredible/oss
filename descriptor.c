#include <stdio.h>
#include <stdlib.h>

#include "descriptor.h"

void initDescriptor(Descriptor *descriptor) {
	int i, j;
	for (i = 0; i < MAX_PROCESSES; i++)
		for (j = 0; j < MAX_RESOURCES; j++)
			descriptor->allocation[i][j] = 0;
	for (i = 0; i < MAX_RESOURCES; i++) {
		if (rand() % 5 == 0) {
			descriptor->resource[i] = 5;
			descriptor->shared[i] = true;
		} else {
			descriptor->resource[i] = 1;//rand() % 10 + 1;
			descriptor->shared[i] = false;
		}
		descriptor->available[i] = descriptor->resource[i];
	}
}

void printVector(char *title, int vector[], int size) {
	int i;
	printf("%-20s\n [ ", title);
	for (i = 0; i < size - 1; i++)
		printf("%2d, ", vector[i]);
	printf("%2d ]\n", vector[i]);
}

void printMatrix(char *title, int matrix[][MAX_RESOURCES], int processes, int resources) {
	int row, col;
	printf("%s\n    ", title);
	for (col = 0; col < resources; col++)
		printf("R%-2d ", col);
	printf("\n");
	for (row = 0; row < processes; row++) {
		printf("P%-2d ", row);
		for (col = 0; col < resources; col++) {
			printf("%-3d ", matrix[row][col]);
		}
		printf("\n");
	}
}

void printDescriptor(Descriptor descriptor, int processes, int resources) {
	printVector("Resource", descriptor.resource, resources);
	printVector("Available", descriptor.available, resources);
	printMatrix("Maximum", descriptor.maximum, processes, resources);
	printMatrix("Allocation", descriptor.allocation, processes, resources);
}
