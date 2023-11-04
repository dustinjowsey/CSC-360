//linked list Node for storing background processes
#ifndef LIST_H
#define LIST_H

#include <sys/types.h>
#include <malloc.h>
#include <unistd.h>
#include <stdio.h>

/*
node structure for creating a linked list
*/

typedef struct Node{
	pid_t pid;
	int state;
	struct Node* next;
	char name[50];
} Node;

/*
insert method to add a new node and its data to a linked list
(background processes)
returns head of list
*/

Node* insert(Node* head, pid_t pid, char* name);

/*
remove method to remove a node from a linked list with pid = pid parameter
(kill background process)
returns head of list
*/

Node* remove_node(Node* head, pid_t pid);

/*
search method used to search for a specific node with pid = pid parameter.
(used to display if a process is stopped)
*/

Node* search_node(Node* head, pid_t pid);

/*
used to change the state of a background process with pid = pid parameter
returns head of list
*/

int node_count(Node* head);



#endif
