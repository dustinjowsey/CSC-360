#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

Queue* createQueue(){
	struct Queue* queue = (struct Queue*)malloc(sizeof(struct Queue));
	queue->numElements = 0;
	queue->first = NULL;
	queue->last = NULL;

	return queue;
}

void insert(Queue* queue, Node* node){
	if(queue->first == NULL){
		queue->first = node;
		queue->last = node;
		queue->numElements = queue->numElements + 1;
		return;
	}
	queue->last->next = node;
	queue->last = node;
	queue->numElements = queue->numElements + 1;
	return;
}

Node* pop(Queue* queue){
	Node* temp = queue->first;
	if(queue->first == NULL){
		return NULL;
	}else{
		queue->first = queue->first->next;
	}
	if(queue->first == NULL){
		queue->last = NULL;
	}
	queue->numElements--;
	return temp;
}

int queueLength(Queue* queue){
	return queue->numElements;
}

int isEmpty(Queue* queue){
	if(queue->numElements == 0){
		return 1;
	}
	return 0;
}

int getHead(Queue* queue){
	return queue->first->id;
}
