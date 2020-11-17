#ifndef MSQ_H
#define MSQ_H

#include <stdbool.h>

typedef struct {
	long type;
	int rid;
	int action;
	int pid;
	int sender;
} msg_t;

int msq_allocate(int);
void msq_send(int, msg_t*, bool);
void msq_receive(int, msg_t*, bool);
void msq_deallocate(int);

#endif
