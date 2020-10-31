#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct {
	int front, rear, size;
	unsigned capacity;
	int *array;
} Queue;

Queue *queue_create(unsigned int);
void queue_push(Queue*, int);
int queue_pop(Queue*);
int queue_peek(Queue*);
bool queue_full(Queue*);
bool queue_empty(Queue*);

#endif
