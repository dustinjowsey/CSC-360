//linked list for storing background processes
#include "list.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>

Node* insert(Node* head, pid_t pid, char* name){

	struct Node* node = (struct Node*)malloc(sizeof(struct Node));
	node->next = NULL;
	node->pid = pid;
	node->state = 1;
	int count = 0;
	name++;
	while(*name != '\0'){
		node->name[count] = *name;
		name++;
		count++;
	}

	if(head == NULL){
		head = node;
	}else{
		struct Node* cur = head;
		while(cur->next != NULL){
			cur = cur->next;
		}
		cur->next = node;

	}

	return head;

}

Node* remove_node(Node* head, pid_t pid){
	if(head == NULL){
		printf("head is null\n");
		printf("Error terminating: no process with pid %d exists\n", pid);
		return NULL;
	}

	struct Node* cur = head;
	struct Node* next_node = head->next;

	if(head->pid == pid){
		head = head->next;
		free(cur);
		return head;
	}
	while(next_node != NULL){
		if(next_node->pid == pid){
			cur->next = next_node->next;
			free(next_node);
			return head;
		}
		cur = next_node;
		next_node = next_node->next;
	}
	return NULL;
}

Node* search_node(Node* head, pid_t pid){
	if(head == NULL){
		return NULL;
	}else{
		while(head != NULL){
			if(head->pid == pid){
				return head;
			}
			head=head->next;
		}
	}
	return NULL;
}

int node_count(Node* head){
	int count = 0;
	while(head != NULL){
		count++;
		head=head->next;
	}
	return count;
}
