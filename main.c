/**
* @file main.c
* @brief A shell that reads and runs commands form the command line
* @date 2020-09-11
* @author Isac Silva Amado - 2191908; Filipe João Seiça de Sousa - 2191168
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#include "debug.h"
#include "memory.h"
#include "args.h"

#define MAX 1000
#define ARG 64

char launchtime[25];
int exec_var = 0;
int stdo_var = 0;
int stdr_var = 0;

int readCommand();
int parseCommand(char command[MAX], int n);
void normalShell();
void maxShell(int max);
void fileShell();
void executeCommand(char *arg[], char *red[2], FILE *std);
void sigFile();
void receivesignal(int signal);

void receivesignal(int signal)
{
	int aux = errno;

	if(signal == SIGINT) {
		printf("\nSignal SIGINT received from: %d - terminating.\n", signal);
        exit(1);
	}
    if(signal == SIGUSR1) {
   		printf("\n%s\n", launchtime);
   		printf("nanoShell$ ");
		fflush( stdout );
	}
    if(signal == SIGUSR2) {
		
        FILE *fname;
        time_t rawtime;
		struct tm *tm;
		time( &rawtime );
		tm = localtime( &rawtime );
		char name[40];
		strftime(name,40,"nanoShell_status_%Y.%m.%d_%Hh%M.%S.txt", tm);
	    fname = fopen(name, "w");
		
	    if(fname == NULL)
        {
  		    fprintf(stderr, "\nCan't create '%s'!\n", name);
  		    exit(1);
  	    }
	
	    fprintf(fname,"\n%d executions of applications\n", exec_var);
		fprintf(fname,"%d executions with STDOUT redir\n", stdo_var);
		fprintf(fname,"%d execution with STDERR redir\n", stdr_var);

	    fclose(fname);
	}

	errno = aux;
}

int main(int argc, char *argv[]){
	
	struct gengetopt_args_info args;
	struct sigaction act;

	if(cmdline_parser(argc, argv, &args)){
		ERROR(1, "ERROR: while executing cmdline_parser\n");
	}
	if(args.max_given && args.file_given){
		ERROR(1, "ERROR: argument --max is incopatible whit argument --file");
	}
	
	time_t rawtime;
	struct tm *tm;
	time( &rawtime );
	tm = localtime( &rawtime );
	strftime(launchtime,20,"%Y-%m-%dT%X+01:00", tm);

	if(args.signalfile_given){
		sigFile();
	}
	
	act.sa_handler = receivesignal;

	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	act.sa_flags |= SA_RESTART; 

	if(sigaction(SIGINT, &act, NULL) < 0){
	  ERROR(1, "sigaction - SIGINT");
	}
    if(sigaction(SIGUSR1, &act, NULL) < 0){
	  ERROR(1, "sigaction - SIGUSR1");
	}
    if(sigaction(SIGUSR2, &act, NULL) < 0){
	  ERROR(1, "sigaction - SIGUSR2s");
	}
	
	if(args.max_given){
		if(args.max_arg > 0){
			maxShell(args.max_arg);
		}
		ERROR(1, "ERROR: invalid value ‘<int>’ for -m/--max.");
	}else if(args.file_given){
		fileShell(args.file_arg);
	}else{
		normalShell();
	}
	cmdline_parser_free(&args);
	return 0;
}

void normalShell(){ //runs the shell until it receives the command 'bye'
	
	int bye = 0;
	while(bye == 0){
		bye = readCommand();
	}
}

void maxShell(int max){//runs the shell until the max executions reached or receives the command 'bye'
	
	int bye = 0;
	while(max > 0 && bye == 0){
		bye = readCommand();
		max--;
	}
	printf("[INFO] max command number reached. Terminating nanoShell");
}

void fileShell(char fileName[]){//reads the given file line by line and runs the commands found
	
	FILE *file;
	file = fopen(fileName, "r");
	
	if(file == NULL) {
      ERROR(1, "cannot open file %s", fileName);
	}
	
	int i = 1, bye = 0 ;
	char line[MAX];
	while(fgets(line, MAX, file) != NULL && bye == 0){
		if(line[0] != '#'){		//if the line is not a comment then parse it and run
			printf("[command #%d]: %s", i, line);
			int n = strlen(line);
			bye = parseCommand(line, n);
			i++;
		}	
	}
	fclose(file);
}

int readCommand(){ //reads the command from stdin
	
	char buf[MAX];
	ssize_t n;
	
	do{
		fflush(stdin);
		printf("nanoShell$ ");
		fflush(stdout);
		n = read(0, buf, MAX);
	}while(n == 1); //reads until there is more than \n
	
	if (n < 0){ //when there is an error on read
		ERROR(2, "\nInput read error.\n");
		return 1;
	}
	
	return parseCommand(buf, n);
}

int parseCommand(char command[MAX], int n){ //parses the command so that it can be run through execvp
	
	int i = 0;
	
	command[n-1]='\0'; //changes the new line character \n into the end of the string
	
	if(strcmp(command, "bye") == 0){ //ends the shell in case the command is 'bye'
		printf("\n[INFO] bye command detected. Terminating nanoShell\n");
		return 1;
	}
	
	if(strpbrk(command, "\"'*?|") != NULL){ //if there are any of the unsuported characteres in the command shows an error
		printf("\n[ERROR] Wrong request '%s'\n", command);
		return 0;
	}
	
	char *arg[ARG];
	char *red[2];
	FILE *std = NULL;
	arg[i] = strtok(command, " ");
		
	do{ //seperates the received command and saves it onto an array 
		i++;
		char *ptr = strtok(NULL, " ");
		arg[i] = NULL;
		if(ptr != NULL){	//if any of the tokens is one of the redirection commands saves the necessary information for it to happen
			if(strcmp(ptr, "2>>") == 0){
				red[0] = "w+";
				red[1] = strtok(NULL, " ");
				std = stderr;
				break;
			}if(strcmp(ptr, "2>") == 0){
				red[0] = "w";
				red[1] = strtok(NULL, " ");
				std = stderr;
				break;
			}if(strcmp(ptr, ">>") == 0){
				red[0] = "w+";
				red[1] = strtok(NULL, " ");
				std = stdout;
				break;
			}if(strcmp(ptr, ">") == 0){
				red[0] = "w";
				red[1] = strtok(NULL, " ");
				std = stdout;
				break;
			}
			arg[i] = ptr; //only hapens if the token is not a redirect command
		}
	}while(arg[i] != NULL); //if NULL means that there are no more arguments in the command received

	executeCommand(arg, red, std);
	return 0;
}

void executeCommand(char *arg[], char *red[2], FILE *std){ //creates a child process to execute the command
	
	pid_t pid;
	if(std == stderr){ //increments to the respective variable
		stdr_var++; 
	}else if(std == stdout){
		stdo_var++;
	}else{
		exec_var++;
	}
	switch (pid = fork()){
			case -1: //Error
				ERROR(1,"Error on function fork\n");
				break;
			case 0: //Filho
				if(std != NULL){ //checks to know whether to redirect or not

					freopen(red[1], red[0], std); //redirects
				}
				execvp(arg[0], arg); //runs the command
				ERROR(1, "Error on function execvp\n");
				break;
			default: //Pai
				wait(NULL); //wait until the child finish executing
				return;
			break;
		}
}

void sigFile(){ //outputs signal info to a file 
	FILE *fname;
	fname = fopen("signals.txt", "w"); //creats file

	if(fname == NULL)
    {
  		ERROR(1, "Can't create 'signals.txt'!\n");
  		exit(1);
  	}
	
	fprintf(fname, "Kill - SIGINT %d\n", getpid()); //writes the signal information
	fprintf(fname, "Kill - SIGUSR1 %d\n", getpid());
	fprintf(fname,"Kill - SIGUSR2 %d\n", getpid());

	printf("[INFO] created file 'signals.txt'\n");

	fclose(fname); //closes file
}
