#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h> 
#include <string.h> 
#include <stdbool.h>
#include <sys/wait.h> 
#include <sys/signal.h>
#include <readline/readline.h> 
#include <readline/history.h>
#include "commands.h"

// global variable to toggle between foreground only mode
volatile sig_atomic_t e_flag = 0; 

// SIGTSTP handler that turns on foreground only mode
void handle_SIGTSTP_f(int signo)
{
	e_flag = 1;
	char *message = "Entering foreground-only mode (& is now ignored)\n";
	write(STDOUT_FILENO, message, 49);
	fflush(stdout);
}

// SIGTSTP handler that exits foreground only mode
void handle_SIGTSTP(int signo)
{
    e_flag = 0;
	char *message = "Exiting foreground-only mode\n";
	write(STDOUT_FILENO, message, 29);
	fflush(stdout);
}

int main (int argc, char **argv)
{
	char commandPrompt[2048]; // store user input
	struct commandLine *currCommand; //to store current command struct
	int status = 0;  //initialize exit status to 0
	int i; // counter
	pid_t pid; // store pid of background processes
	pid_t processes[100]; // initialize list of started processes (max 100 background processes)
	int size = 100;	// size of processes list
	// set the list of background processes to 0
	memset(processes, 0, sizeof(processes));

	// initialize struct for signal handlers
	struct sigaction ignore_action = {0}, SIGTSTP_action_f = {0}, SIGTSTP_action = {0};
	
	// signal type should be ignored
	ignore_action.sa_handler = SIG_IGN;

	// SIGTSTP_action_f struct to enter foreground-only mode
	SIGTSTP_action_f.sa_handler = handle_SIGTSTP_f;

	// SIGTSTP_action to exit foreground-only mode
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	
	// ignore SIGINT in parent shell
	sigaction(SIGINT, &ignore_action, NULL);

	while(true)
	{
		// checks background processes for completion 
		if (processes[0] != 0) checkBackgroundProcesses(processes);
		
		// sets SIGTSTP signal to toggle between foreground-only mode
		if (e_flag == 1) sigaction(SIGTSTP, &SIGTSTP_action, NULL);
		if (e_flag == 0) sigaction(SIGTSTP, &SIGTSTP_action_f, NULL);
	
		// colon prompt for command line
		printf(": ");
		fflush(stdout);

		// read command from user input
		fgets(commandPrompt, sizeof commandPrompt, stdin); 

		// if user enters a blank line or #, ignore
		if(strncmp(commandPrompt, "\n", 1) == 0 || strncmp(commandPrompt, "#", 1) == 0) continue;
		
		//if there are any instances of $$
		char *ptr;
		char *orig = commandPrompt; 
		char newCommandPrompt[2048];
    	newCommandPrompt[0] = '\0';
		char *newCommand;

		// check if there is an instance of $$
		ptr = strstr(orig, "$$");		

		// if there is $$ variable
		if (ptr != NULL) 
		{
			// expand the $$ variable to pid
			newCommand = expandCommand(ptr, orig, newCommandPrompt);
			memcpy(commandPrompt, newCommand, strlen(newCommand)+1);
		} 

		//remove the /n from stdin
		commandPrompt[strcspn(commandPrompt, "\n")] = 0;

		// create command structure
		currCommand = createCommand(commandPrompt);

		// if user exits, kill any background processes, and quit
		if (strcmp(currCommand->arg[0], "exit") == 0) 
		{	
			int i = 0;
			while(processes[i] != 0 && processes[i] != -1)
			{
				kill(processes[i], SIGTERM);
				i++;
			}
			break;
		}
		// if command is cd
		if (strcmp(currCommand->arg[0], "cd") == 0) 
		{
			cd(currCommand);
			continue;
		}

		// if command is status, print exit status of last foreground process 
		if (strcmp(currCommand->arg[0], "status") == 0) 
		{
			printf("exit value %d\n", status);
			fflush(stdout);
			continue;
		}
	
		// non-built-in commands
		
		// check if foreground only mode
		if (e_flag == 1) currCommand->bg = false;

		// if foreground process, return exit status/termination signal
		if (currCommand->bg != true) 
		{
			status = foregroundProcess(currCommand);
			fflush(stdout);
		}
			
		//background process
		else 
		{
			// run command, and store pid of process
			pid = backgroundProcess(currCommand);
			fflush(stdout);

			// add pid to array of started processes
			for(int i = 0; i < size; i++)
			{
				if (processes[i] == 0 || processes[i] == -1)
				{
					processes[i] = pid;
					break;
				}
			}	
		}		
	} 
	freeCommand(currCommand);	
	return 0;
}


