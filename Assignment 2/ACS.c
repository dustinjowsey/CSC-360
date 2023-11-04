#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <sys/time.h>
#include <malloc.h>
#include <unistd.h>
#include <semaphore.h>
#include "queue.c"

//Doesn't work if change these numbers really. Would need more work.
#define NUMBEROFQUEUES 2
#define NUMBEROFCLERKS 5

void printError(char* type, int errnum);

void* customer_entry(void* custInfo);
void* clerk_entry(void* clerkNum);

int queueStatus;
int numClerksAvailable = NUMBEROFCLERKS;
int queueTotalLength[NUMBEROFQUEUES];
int headOfQueue[NUMBEROFQUEUES];
double waitingTime[NUMBEROFQUEUES];

double initTime;

pthread_cond_t customerThreadExit;
pthread_cond_t servedCustomer;
pthread_cond_t custWaitCond;

Queue* queue[NUMBEROFQUEUES];

pthread_mutex_t customerThreadExitMutex;
pthread_mutex_t queueHead;
pthread_mutex_t clerksAvailableMutex;
pthread_mutex_t buisQueueMutex;
pthread_mutex_t stndQueueMutex;
pthread_mutex_t oneClerkOnly;

int main(int argc, char *argv[]){
	fprintf(stdout, "***Started simulation***\n");
	const int LINE_LENGTH = 15;
	int numCustomers;

	if(argc > 2){
		fprintf(stdout, "Error: Too many arguments, please provide arguments as follows:\n./ACS 'input.txt'\n");
		exit(EXIT_FAILURE);
	}else if(argc < 2){
		fprintf(stdout, "Error: Too few arguments, please provide arguments as follows:\n./ACS 'input.txt'\n");
		exit(EXIT_FAILURE);
	}

	FILE* fp = fopen(argv[1], "r");
	if(fp == NULL){
		fprintf(stdout, "Error: Input file could not be opened!\n");
		exit(EXIT_FAILURE);
	}
	//create queues
	for(int i = 0; i < NUMBEROFQUEUES; i++){
		queue[i] = createQueue();
	}

	char line[LINE_LENGTH];
	fgets(line, LINE_LENGTH, fp);
	numCustomers = atoi(line);
	Node* custInfo = (struct Node*)malloc((numCustomers+1) * sizeof(struct Node));
	Node* node = (struct Node*)malloc(sizeof(struct Node));
	char* tok = NULL;
	while(fgets(line, LINE_LENGTH, fp) != NULL){
		tok = strtok(line, ":");
		node->id = atoi(tok);
		tok = strtok(NULL, ",");
		node->clas = atoi(tok);
		tok = strtok(NULL, ",");
		node->arrivalTime = atof(tok);
		tok = strtok(NULL, "\n");
		node->serviceTime = atof(tok);
		custInfo[node->id] = *node;
	}

	int retVal;
	char* type;

	retVal = fclose(fp);
	if(retVal != 0){
		type = "Fileclose";
		printError(type, errno);
	}

	//initialize conditions
	retVal = pthread_cond_init(&customerThreadExit, NULL);
	if(retVal != 0){
		type = "customerThreadExit";
		printError(type, retVal);
	}

	retVal = pthread_cond_init(&servedCustomer, NULL);
	if(retVal != 0){
		type = "servedCustomer";
		printError(type, retVal);
	}

	retVal = pthread_cond_init(&custWaitCond, NULL);
	if(retVal != 0){
		type = "custWaitCond";
		printError(type, retVal);
	}

	//initialize mutexs
	retVal = pthread_mutex_init(&customerThreadExitMutex, NULL);
	if(retVal != 0){
		type = "customerThreadExitMutex";
		printError(type, retVal);
	}

	retVal = pthread_mutex_init(&queueHead, NULL);
	if(retVal != 0){
		type = "queueHead";
		printError(type, retVal);
	}

	retVal = pthread_mutex_init(&clerksAvailableMutex, NULL);
	if(retVal != 0){
		type = "clerksAvailableMutex";
		printError(type, retVal);
	}

	retVal = pthread_mutex_init(&buisQueueMutex, NULL);
	if(retVal != 0){
		type = "buisQueueMutex";
		printError(type, retVal);
	}

	retVal = pthread_mutex_init(&stndQueueMutex, NULL);
	if(retVal != 0){
		type = "stndQueueMutex";
		printError(type, retVal);
	}

	retVal = pthread_mutex_init(&oneClerkOnly, NULL);
	if(retVal != 0){
		type = "oneCLerkOnly";
		printError(type, retVal);
	}

	//create threads
	int* clerkNumber = (int*)malloc(NUMBEROFCLERKS * sizeof(int));
	pthread_t* custId = (pthread_t*)malloc((numCustomers+1) * sizeof(pthread_t));
	pthread_t* clerkNum = (pthread_t*)malloc((NUMBEROFCLERKS+1) * sizeof(pthread_t));
	for(int i = 1; i <= NUMBEROFCLERKS; i++){
		clerkNumber[i] = i;
		retVal = pthread_create(&clerkNum[i], NULL, clerk_entry, (void*)&clerkNumber[i]);
		if(retVal != 0){
			type = "clerk thread";
			printError(type, retVal);
		}
	}
		//get initial time
	initTime = time(NULL);

	for(int i = 1; i <= numCustomers; i++){
		retVal = pthread_create(&custId[i], NULL, customer_entry, (void*)&custInfo[i]);
		if(retVal != 0){
			type = "customer thread";
			printError(type, retVal);
		}
	}

	//wait for all customer threads to return;
	for(int i = 1; i <= numCustomers; i++){
		retVal = pthread_join(custId[i], NULL);
		if(retVal != 0){
			printf("Error: unable to join threads. returned error number %d\n", retVal);
			exit(EXIT_FAILURE);
		}
	}

	//final calculations
	double overallAvgWaitingTime;
	for(int i = 0; i < NUMBEROFQUEUES; i++){
		overallAvgWaitingTime += waitingTime[i];
	}
	overallAvgWaitingTime = overallAvgWaitingTime/numCustomers;
	fprintf(stdout, "\nThe average waiting time from arriving to finishing service for all customers in the system is: %.2f seconds. \n", overallAvgWaitingTime);
	fprintf(stdout, "The average waiting time from arriving to finishing sercive for all buisness-class customers is: %.2f seconds. \n", (waitingTime[1]/queueTotalLength[1]));
	fprintf(stdout, "The average waiting time from arriving to finishing service for all economy-class customers is: %.2f seconds. \n", (waitingTime[0]/queueTotalLength[0]));

	fprintf(stdout, "***Simulation Finished!***\n");

	return 0;
}

void* customer_entry(void* cusInfo){
	struct Node* info = (struct Node*)cusInfo;
	int queueClass = info->clas;

	sleep(info->arrivalTime/10);
	fprintf(stdout, "A customer arrives: customer ID %2d. \n", info->id);

	//check which mutex to lock and unlock
	if(queueClass == 0){
		pthread_mutex_lock(&stndQueueMutex);
	}else{
		pthread_mutex_lock(&buisQueueMutex);
	}

	fprintf(stdout, "->Customer %d enters a queue: the queue ID %d, and length of the queue before the customer entered is  %d. \n", info->id,queueClass, queueLength(queue[queueClass]));
	queueTotalLength[queueClass]++;
	insert(queue[queueClass], info);
	info->start = difftime(time(NULL), initTime);
	//pthread_mutex_unlock(&acessingTopOfQueue);
	if(queueClass == 0){
		pthread_mutex_unlock(&stndQueueMutex);
	}else{
		pthread_mutex_unlock(&buisQueueMutex);
	}

	while(1 == 1){
		//Used so that we were not getting the head at the same time as poping it.
		if(info->id == getHead(queue[queueClass])){
			//pthread_mutex_unlock(&acessingTopOfQueue);
			//This while loops stops queue 0 if there are buisness class customers in line.
			while((queueClass == 0 && isEmpty(queue[1]) == 0) || numClerksAvailable <= 0){
				pthread_cond_wait(&servedCustomer, &clerksAvailableMutex);
			}
			pthread_mutex_unlock(&clerksAvailableMutex);
			//if at front of queue signal a clerk to wake up
			pthread_mutex_lock(&queueHead);
			//so clerk knows what queue to look for customer
			queueStatus = queueClass;
			pthread_cond_signal(&custWaitCond);

			break;
		}
		//pthread_mutex_unlock(&acessingTopOfQueue);
		//Check if any clerks are free if not wait for one to be else let a clerk know you
		//are ready to be serviced.
		while(numClerksAvailable <= 0 || info->id != getHead(queue[queueClass])){
			pthread_cond_wait(&servedCustomer, &clerksAvailableMutex);
		}
		//unlock right after because there might be more than one clerk available.
		pthread_mutex_unlock(&clerksAvailableMutex);

	}
	//Wait for the clerk to finish serving you before exiting thread.
	//If I do not do this then pthread_join in the main thread occurs too early
	pthread_cond_wait(&customerThreadExit, &customerThreadExitMutex);
	pthread_mutex_unlock(&customerThreadExitMutex);

	pthread_exit(NULL);
	return NULL;

}

void* clerk_entry(void* clerkNum){
	int clerkNumber = *(int*)clerkNum;
	double curTime;
	Node* customer;
	int curQueueStatus;

	//pthread_mutex_lock(&clerksAvailableMutex);
	while(1 == 1){
		//Wait for the customer at the head of the list to signal a clerk.
		pthread_cond_wait(&custWaitCond, &clerksAvailableMutex);
		curQueueStatus = queueStatus;
		pthread_mutex_unlock(&queueHead);

		pthread_mutex_lock(&oneClerkOnly);
		numClerksAvailable--;
		pthread_mutex_unlock(&oneClerkOnly);

		if(curQueueStatus == 0){
			pthread_mutex_lock(&stndQueueMutex);
		}else{
			pthread_mutex_lock(&buisQueueMutex);
		}

		//pthread_mutex_lock(&acessingTopOfQueue);
		customer = pop(queue[curQueueStatus]);
		//let customers know there is a new head of the list
		pthread_cond_broadcast(&servedCustomer);
		//pthread_mutex_unlock(&acessingTopOfQueue);

		if(curQueueStatus == 0){
			pthread_mutex_unlock(&stndQueueMutex);
		}else{
			pthread_mutex_unlock(&buisQueueMutex);
		}

		curTime = difftime(time(NULL), initTime);
		fprintf(stdout, "-->A clerk starts serving a customer: start time %.2f seconds, the customer ID %d, the clerk ID %d\n", curTime/10, customer->id, clerkNumber);
		//get waiting time
		pthread_mutex_lock(&oneClerkOnly);
		waitingTime[customer->clas] += difftime(curTime, customer->start);
		pthread_mutex_unlock(&oneClerkOnly);

		sleep(customer->serviceTime/10);
		curTime = difftime(time(NULL), initTime);
		fprintf(stdout, "-->>A clerk finishes serving a customer: end time %.2f seconds, the customer ID %d, the clerk ID %d.\n", curTime/10, customer->id, clerkNumber);
		//Clerk is now available again
		pthread_mutex_lock(&oneClerkOnly);
		numClerksAvailable++;
		pthread_mutex_unlock(&oneClerkOnly);
		//Signal the customer thread waiting to exit that it can
		pthread_cond_broadcast(&customerThreadExit);
		//Signal all customer threads that a clerk is available.
		pthread_cond_broadcast(&servedCustomer);
	}

	return NULL;
}

//Just to make things a bit cleaner when initializing all mutexes, conds, etc
void printError(char* type, int errnum){
	fprintf(stdout,"Error: unable to initialize/create %s. Returned error number %d\n", type, errnum);
	exit(EXIT_FAILURE);
}
