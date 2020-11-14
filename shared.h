#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <sys/stat.h>

#define BUFFER_LENGTH 1024
#define EXIT_STATUS_OFFSET 20

#define PERMS (S_IRUSR | S_IWUSR)

enum ActionType { REQUEST, RELEASE, TERMINATE, GRANT, DENY };

typedef struct {
	long type;
	int rid;
	int action;
	int pid;
	int sender;
} Message;

typedef struct {
	int available;
	int claim[20];
	int allocation[20];
	bool shareable;
} Descriptor;

typedef struct {
} PCB;

void init(int, char**);
char *getProgramName();
void error(char*, ...);
void crash(char*);
void output(char*, ...);

#endif
