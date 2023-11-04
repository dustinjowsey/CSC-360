//Queue for tracking customers in line and servicing those customers
#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>

/*
A node structure to keep track of each customers id and service time.
The node structure also helps track who is next in line as well as the clerk that served the customer.
*/
typedef struct Node{
	int id;
	float arrivalTime;//in seconds Both times get converted when sleep is called
	float serviceTime;//in seconds
	int clas;
	time_t start;
	struct Node* next;
} Node;

/*
A queue structure to store all customers(nodes) who are waiting in line.
We can also track number of elements to determine if the cashiers should be idle
*/
typedef struct Queue{
	int numElements;
	struct Node* first, *last;
} Queue;

/*
creates an empty queue
*/
Queue* createQueue();

/*
Inserts a customer into the end of the queue
*/
void insert(Queue* queue, Node* node);

/*
Removes the first customer from the queue
*/
Node* pop(Queue* queue);

/*
returns the current length of the queue
*/
int queueLength(Queue* queue);

/*
returns true if the queue is empty
*/
int isEmpty(Queue* queue);

/*
returns the id of the node at the front of the queue
*/
int getHead(Queue* queue);

#endif
