#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>

#define BUFFER_LENGTH 1024
#define EXIT_STATUS_OFFSET 20

typedef struct {
	long type;
	char text[BUFFER_LENGTH];
} Message;

typedef struct {
	long s;
	long ns;
} Time;

typedef struct {
	int available;
	int claim[20];
	int allocation[20];
	bool shareable;
} Descriptor;

typedef struct {
	Time system;
} OS;

void init(int, char**);
char *getProgramName();
//void error(char*, ...);
//void crash(char*);
void output(char*, ...);

#endif
