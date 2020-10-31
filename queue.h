#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct {
	unsigned int front, rear, size;
	unsigned int capacity;
	int *array;
} Queue;

Queue *queue_create(unsigned int);
void queue_push(Queue*, unsigned int);
unsigned int queue_pop(Queue*);
unsigned int queue_peek(Queue*);
bool queue_full(Queue*);
bool queue_empty(Queue*);

#endif
