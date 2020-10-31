#include <limits.h>
#include <stdlib.h>

#include "queue.h"

Queue *queue_create(unsigned int capacity) {
	Queue *queue = (Queue*) malloc(sizeof(Queue));
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;
	queue->array = (int*) malloc(queue->capacity * sizeof(int));
	return queue;
}

void queue_push(Queue *queue, unsigned int item) {
	if (queue_full(queue)) return;
	queue->rear = (queue->rear + 1) % queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
}

unsigned int queue_pop(Queue *queue) {
	if (queue_empty(queue)) return -1;
	int item = queue->array[queue->front];
	queue->front = (queue->front + 1) % queue->capacity;
	queue->size = queue->size - 1;
	return item;
}

unsigned int queue_peek(Queue *queue) {
	if (queue_empty(queue)) return -1;
	int item = queue->array[queue->front];
	return item;
}

bool queue_full(Queue *queue) {
	return queue->size == queue->capacity;
}

bool queue_empty(Queue *queue) {
	return queue->size == 0;
}
