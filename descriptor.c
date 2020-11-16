#include <stdio.h>

#include "descriptor.h"

void initDescriptor(Descriptor *descriptor) {
	int i, j;
	for (i = 0; i < MAX_PROCESSES; i++)
		for (j = 0; j < MAX_RESOURCES; j++)
			descriptor->allocation[i][j] = 0;
	for (i = 0; i < MAX_RESOURCES; i++) {
		if (rand() % 5 == 0) {
			descriptor->resource[i] = 5;
			descriptor->available[i] = descriptor->resource[i];
			descriptor->shared[i] = 1;
		} else {
			descriptor->resource[i] = rand() % 10 + 1;
			descriptor->available[i] = descriptor->resource[i];
			descriptor->shared[i] = 0;
		}
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
			if (matrix[row][col] == 0) printf("--  ");
			else printf("%-3d ", matrix[row][col]);
		}
		printf("\n");
	}
}
