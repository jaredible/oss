#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <sys/stat.h>

#define BUFFER_LENGTH 1024
#define EXIT_STATUS_OFFSET 20

#define CLOCK_KEY_ID 0
#define DESCRIPTOR_KEY_ID 1
#define QUEUE_KEY_ID 2
#define PERMS (S_IRUSR | S_IWUSR)

enum Action { REQUEST, RELEASE, TERMINATE, GRANT, DENY };

typedef struct {
	long type;
	int rid;
	int action;
	int pid;
	int sender;
} Message;

void init(int, char**);
char *getProgramName();
void error(char*, ...);
void crash(char*);
void slog(char*, ...);

#endif
