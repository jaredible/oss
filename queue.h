/*
 * queue.h 11/17/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>

typedef struct {
	unsigned int front, rear, size;
	unsigned int capacity;
	int *array;
} Queue;

Queue *queue_create(unsigned int);
void queue_push(Queue*, int);
int queue_pop(Queue*);
int queue_peek(Queue*);
bool queue_full(Queue*);
bool queue_empty(Queue*);
void queue_display(Queue*);

#endif
