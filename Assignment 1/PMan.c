#include "list.c"
#include <sys/types.h>
#include <inttypes.h>
#include <iso646.h>
#include <errno.h>
#include <wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <wait.h>
#include <signal.h>

#define MAX_INPUT_LENGTH 1000

void bg_entry(char** argv);
void bglist_entry(void);
void bgsig_entry(pid_t, char* cmd_type);
void pstat_entry(char* pid);
void usage_pman(char* cmd_type);
void check_zombieProcess(void);

pid_t chartopid(char* string);

Node* head = NULL;

int main(){
	const char* CMD_BG = "bg";
	const char* CMD_BGLIST = "bglist";
	const char* CMD_BGKILL = "bgkill";
	const char* CMD_BGSTOP = "bgstop";
	const char* CMD_BGCONT = "bgstart";
	const char* CMD_PSTAT = "pstat";
	const char* CMD_EXIT = "q";

	const int MAX_ARGUMENTS = 50;

	char cmd[MAX_INPUT_LENGTH];
	char* cmd_type;
	char* argv[MAX_ARGUMENTS];
	char* temp;
	int arg_count;

	while(1){
		arg_count = 0;
		printf("PMan: > ");
		fgets(cmd, MAX_INPUT_LENGTH, stdin);

		cmd_type = strtok(cmd, " ");
		temp = cmd_type;
		//removes newline at end of cmd_type if command is one word.
		while(*temp != '\0'){
			if(*temp == '\n'){
				*temp = '\0';
				break;
			}
			temp++;
		}
		//tokenize all arguments after cmd_type
		argv[arg_count] = strtok(NULL, " ");
		while(argv[arg_count] != NULL){
			arg_count++;
			argv[arg_count] = strtok(NULL, " ");
		}
		arg_count--;
		temp = argv[arg_count];
		//remove new line on last argument
		while(*temp != '\0'){
			if(*temp == '\n'){
				*temp = '\0';
				break;
			}
			temp++;
		}
		//checks what command was entered
		if(strcmp(cmd_type, "") == 0){ //does not return an empty command as unknown command for a cleaner display
		}			       //of processes killed outside of PMan
		else if (strcmp(cmd_type, CMD_BG) == 0){
			bg_entry(argv);
		}
		else if(strcmp(cmd_type, CMD_BGLIST) == 0){
			bglist_entry();
		}
		else if(strcmp(cmd_type, CMD_BGKILL) == 0 || strcmp(cmd_type, CMD_BGSTOP) == 0 || strcmp(cmd_type, CMD_BGCONT) == 0){
			pid_t pid  = chartopid(argv[0]);
			//if pid < 0 the pid entered could not be converted to type pid_t
			if(pid < 0){
				continue;
			}
			bgsig_entry(pid, cmd_type);
		}
		else if(strcmp(cmd_type, CMD_PSTAT) == 0){
			pstat_entry(argv[0]);
		}
		else if(strcmp(cmd_type, CMD_EXIT) == 0){
			//way to exit PMan
			break;
		}
		else {
			usage_pman(cmd_type);
		}
		check_zombieProcess();
	}
	return 0;
}


/*
Mentioned in referneces, used to convert a string into type pid_t
*/
pid_t chartopid(char* string){
	intmax_t xmax;
	char* temp;

	errno = 0;
	xmax = strtoimax(string, &temp, 10);
	if(errno != 0 || temp == string || *temp != '\0' || xmax != (pid_t)xmax){
		printf("'%s' cannot be converted to a valid pid\n", string);
		return -1;
	}else{
		return (pid_t)xmax;
	}

}

void bg_entry(char** argv){
	pid_t pid;
	pid = fork();
	if(pid == 0){
		if(execvp(argv[0], argv) < 0){
			perror("Error on execvp");
		}
		exit(EXIT_SUCCESS);
	}
	else if(pid > 0) {
		// store information of the background child process in your data structures
		if(argv[0] == NULL){
			printf("Error: no program entered.\n");
			return;
		}
		head = insert(head, pid, argv[0]);
	}
	else {
		perror("fork failed");
		exit(EXIT_FAILURE);
	}
}

void bglist_entry(void){
	Node* cur = head;
	//print out all process in the linked list i.e background processes
	while(cur != NULL){
		printf("%d: %s", cur->pid, cur->name);
		//if cur->state equals 0, want to display stopped beside a process
		if(cur->state == 0){
			printf(" (Stopped)\n");
		}else{
			printf("\n");
		}
		cur = cur->next;
	}
	printf("Running background processes: %d\n", node_count(head));
}

void bgsig_entry(pid_t pid, char* cmd_type){
	Node* process;
	int retVal = 0;
	if(strcmp(cmd_type, "bgkill") == 0){
		retVal = kill(pid, 9);
		if(retVal == -1){
			perror("Error on kill");
			return;
		}
	}else if(strcmp(cmd_type, "bgstop") == 0){
		//checks if process is already in a stopped state.
		process = search_node(head, pid);
		if(process->state == 0 && process != NULL){
			printf("Process %d is already stopped!\n", pid);
			return;
		}

		retVal = kill(pid, 19);
		if(retVal == -1){
			perror("Error on kill");
			return;
		}
		//sets process to a stopped state (used for bglist)
		process->state = 0;
		printf("Process %d successfully stopped!\n", pid);
	}else{
		//checks if process is already running.
		process = search_node(head, pid);
		if(process->state == 1 && process != NULL){
			printf("Process %d is already running!\n", pid);
			return;
		}

		retVal = kill(pid, 18);
		if(retVal == -1){
			perror("Error on kill");
			return;
		}
		//sets process to a running state (used for bglist)
		process->state = 1;
		printf("Process %d successfully continued running!\n", pid);
	}
}

void pstat_entry(char* pid){
	//check if the process entered is within pman
	pid_t pid_of_type = chartopid(pid);
	if(search_node(head, pid_of_type) == NULL){
		printf("Error: process %d does not exist in PMan.\n", pid_of_type);
		return;
	}

	FILE* fp;
	char info_stat[MAX_INPUT_LENGTH];
	char info_status[MAX_INPUT_LENGTH];
	char* data;
	float time;

	char path_stat[MAX_INPUT_LENGTH] = {"/proc/"};
	char path_status[MAX_INPUT_LENGTH] = {"/proc/"};
	char path_stat_end[5] = {"/stat"};
	char path_status_end[7] = {"/status"};
	//an array of strings to easily display each type datas name
	char* data_type[255] = {"comm", "state", "utime", "stime", "rss", "voluntary_ctxt_switches", "nonvoluntary_ctxt_switches"};
	int data_type_count = 0;
	//creates paths to both status and stat of the process pid
	int count = 6;
	while(*pid != '\0'){
		path_stat[count] = *pid;
		path_status[count] = *pid;
		count++;
		pid++;
	}
	for(int i = 0; i <= 6; i++){
		if(i <= 4){
			path_stat[count] = path_stat_end[i];
		}
		path_status[count] = path_status_end[i];
		count++;
	}

	fp = fopen(path_stat, "r");

	if(fp == NULL){
		printf("Error: process does not exist.\n");
		return;
	}
	fgets(info_stat, 1000, fp);

	data = strtok(info_stat, " ");
	count = 0;
	//parse and display required data of process stat file
	while(count <= 23){
		if(count == 1 || count == 2 || count == 13 || count == 14 || count == 23){
			if(count == 13 || count == 14){
				time = (atof(data)/sysconf(_SC_CLK_TCK));
				printf("%s: %f\n", data_type[data_type_count], time);
				data_type_count++;
			}else{
				printf("%s: %s\n", data_type[data_type_count], data);
				data_type_count++;
			}
		}
		data = strtok(NULL, " ");
		count++;
	}
	fclose(fp);

	fp = fopen(path_status, "r");
	if(fp == NULL){
		printf("Error: process no longer exists (terminated while running pstat?)\n");
		return;
	}

	count = 0;
	//parse and display required data of process status file
	while(fscanf(fp, "%[^\n]\n", info_status) != EOF){
		if(count == 53 || count == 54){
			data = strtok(info_status, ":");
			data = strtok(NULL, "\n");
			//delete leading spaces and tabs from the swithes data
			while(*data == ' ' || *data == '\t'){
				data++;
			}
			printf("%s: %s\n", data_type[data_type_count], data);
			data_type_count++;
		}
		count++;
	}
	fclose(fp);
return;

}

void usage_pman(char* cmd_type){
	//list of all commands in PMan if --help is entered.
	if(strcmp(cmd_type, "--help") == 0){
		printf("Command list:\n\n bg ./<program name> <arg1> <arg2>... - executes a program within the same directory named 'program name' with arguments 'arg1' 'arg2' etc.\n");
		printf("\n bglist - lists all process currently running by PMan along with their pid.\n");
		printf("\n bgkill <pid> - kills the process with a matching pid to the inputted one.\n");
		printf("\n bgstop <pid> - stops the process with a matching pid to the inputted one.\n");
		printf("\n bgstart <pid> - starts a stopped process with a matching pid to the inputted one.\n");
		printf("\n pstat <pid> - displays comm, state, utime, stime, rss, voluntary_ctxt_switches, and nonvoluntary_ctxt_switches of the process with a matching pid to the inputted one.\n");
		printf("\n q - exits PMan.\n\n");
	}else{
	printf("%s: command not found, type '--help' for a list of commands\n", cmd_type);
	}
	return;
}

void check_zombieProcess(void){
	int status;
	int retVal = 0;
	while(1) {
		usleep(1000);
		if(head == NULL){
			return ;
		}
		retVal = waitpid(-1, &status, WNOHANG);
		if(retVal > 0) {
			//remove the background process from your data structure
			if(WIFSIGNALED(status)){
				printf("Process %d was killed successfully\n", retVal);
				head = remove_node(head, retVal);
			}else if(WIFEXITED(status)){
				printf("Process %d exited successfully\n", retVal);
				head = remove_node(head, retVal);
			}
			return;
		}
		else if(retVal == 0){
			break;
		}
		else{
			perror("waitpid failed");
			exit(EXIT_FAILURE);
		}
	}
	return ;
}
