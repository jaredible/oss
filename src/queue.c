/*
 * queue.c 11/17/20
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

queue_t *queue_create(unsigned int capacity) {
	queue_t *queue = (queue_t*) malloc(sizeof(queue_t));
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;
	queue->array = (int*) malloc(queue->capacity * sizeof(int));
	return queue;
}

void queue_push(queue_t *queue, int item) {
	if (queue_full(queue)) return;
	queue->rear = (queue->rear + 1) % queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
}

int queue_pop(queue_t *queue) {
	if (queue_empty(queue)) return -1;
	int item = queue->array[queue->front];
	memset(&queue->array[queue->front], 0, sizeof(int));
	queue->front = (queue->front + 1) % queue->capacity;
	queue->size = queue->size - 1;
	return item;
}

int queue_peek(queue_t *queue) {
	if (queue_empty(queue)) return -1;
	int item = queue->array[queue->front];
	return item;
}

bool queue_full(queue_t *queue) {
	return queue->size == queue->capacity;
}

bool queue_empty(queue_t *queue) {
	return queue->size == 0;
}

void queue_display(queue_t *queue) {
	int i = queue->front;
	while (i != queue->rear) {
		printf("%2d ", queue->array[i]);
		i = (i + 1) % queue->capacity;
	}
	printf("%2d\n", queue->array[i]);
}
