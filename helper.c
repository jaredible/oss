#include <math.h>
#include <stdio.h>

#include "helper.h"

int getLineCount(char *path) {
	FILE *fp = fopen(path, "r");
	if (fp == NULL) crash("fopen");
	
	int count = 0;
	char c;
	for (c = getc(fp); c != EOF; c = getc(fp))
		if (c == '\n') count++;
	
	fclose(fp);
	
	return count;
}

int getQueueQuantum(int baseQuantum, int queue) {
	if (queue > 0 && queue < (QUEUE_SET_COUNT - 1)) {
		return baseQuantum * pow(2, queue);
	} else {
		return -1;
	}
}

int getUserQuantum(int baseQuantum, int queue) {
	return baseQuantum / pow(2, queue);
}

void here(int n) {
	if (DEBUG) printf("%s: HERE %d\n", getProgramName(), n);
}

void printBits(unsigned long int bits, int size) {
	unsigned int i;
	for (i = 1 << (size - 1); i > 0; i /= 2)
		(bits & i) ? printf("1") : printf("0");
	printf("\n");
}
