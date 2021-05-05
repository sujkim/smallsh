#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h> 
#include <string.h> 
#include <stdbool.h>
#include <sys/wait.h> 
#include <readline/readline.h> 
#include <readline/history.h>
#include <signal.h>
#include "commands.h"


/* Replaces the variable $$ with process ID of smallsh itself */
char *expandCommand(char *ptr,char *orig, char *newCommandPrompt)
{     
    int size;
    char pid[10];

    while (true)
    {
        if (ptr == NULL) 
        {   
            //if end of prompt, add everything after $$
            strncat(newCommandPrompt, orig, strlen(orig));
            return newCommandPrompt;
        }

        // keep track of size of anything before any instance of $$
        size = strlen(orig) - strlen(ptr);

        // convert pid to string
        sprintf(pid, "%d", getpid());

        // add anything that was before $$
        strncat(newCommandPrompt, orig, size);

        // add pid
        strncat(newCommandPrompt, pid, strlen(pid));

        // move pointer to after $$
        orig += size + 2;

        // check for another instance
        ptr = strstr(orig, "$$");
    }
}

/* creates a commandLine struct given user input */
struct commandLine *createCommand(char *commandPrompt)
{
    struct commandLine *currCommand = malloc(sizeof(struct commandLine));

    // for use with strtok_r
    char *saveptr;

    // first token is the command
    char *token = strtok_r(commandPrompt, " ", &saveptr);
    currCommand->arg[0] = calloc(strlen(token)+1, sizeof(char));
    strcpy(currCommand->arg[0], token);

    // get next token
    token = strtok_r(NULL, " ", &saveptr);

    int i = 1;
    // if next token is an argument, add any arguments to arg array
    while (token != NULL && strcmp(token, "<") != 0 && strcmp(token, ">") != 0)
    {  
        currCommand->arg[i] = calloc(strlen(token)+1, sizeof(char));
        strcpy(currCommand->arg[i], token);
        token = strtok_r(NULL, " ", &saveptr);
        i++;
    }

    // if there are redirections < or >
    while (token != NULL && strcmp(token, "&") != 0)
    {
        // if next token is <, get next token and add as input file
        if (strcmp(token, "<") == 0)
        {
            token = strtok_r(NULL, " ", &saveptr);
            currCommand->inputFile = calloc(strlen(token)+1, sizeof(char));
            strcpy(currCommand->inputFile, token);
        }

        // if next token is >, get next token and add as output file
        else if (strcmp(token, ">") == 0)
        {
            token = strtok_r(NULL, " ", &saveptr);
            currCommand->outputFile = calloc(strlen(token)+1, sizeof(char));
            strcpy(currCommand->outputFile, token);
        }

        // get next token
        token = strtok_r(NULL, " ", &saveptr);
    }
 
    // check if last token is &
    if (currCommand->inputFile == NULL || currCommand->outputFile == NULL)
    {
        if (strcmp(currCommand->arg[i-1],"&") == 0) 
        {
            currCommand->bg = true;
            currCommand->arg[i-1] = '\0';
        }
    }

    if (token != NULL && strcmp(token, "&") == 0) currCommand->bg = true;
        
    // if no more arguments, exit
    if (token == NULL) return currCommand;

}

/* handles the cd command */
void cd (struct commandLine *currCommand)
{	
    // if no arguments
    if (currCommand->arg[1] == NULL) 
    {
        // go to home directory
        int currDir = chdir(getenv("HOME"));

    }
    // move to specified directory
    else
    {
        int currDir = chdir(currCommand->arg[1]);
    }
}

/* handles any foreground processes and returns the pid of the child process */
pid_t foregroundProcess(struct commandLine *currCommand)
{
    int childStatus;
    char *inputFile;
    char *outputFile;

    //struct to set SIGINT to default action
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_DFL;

    //struct to set SIGTSTP to ignore
    struct sigaction SIGTSTP_child_action = {0};
    SIGTSTP_child_action.sa_handler = SIG_IGN;

    // Fork a new process
    pid_t childPid = fork();

    switch(childPid)
    {
        case -1:
            perror("fork()\n");
            fflush(stdout);
            exit(1);
            break;
        case 0:
            // In the child process

            //set SIGINT to default action in the child process
            sigaction(SIGINT, &SIGINT_action, NULL);

            //set SIGTSTP to ignore in child process
            sigaction(SIGTSTP, &SIGTSTP_child_action, NULL);

            // check for input file
            if (currCommand->inputFile != NULL)
            {
                //open read only
                int inputFile = open(currCommand->inputFile, O_RDONLY);
                if (inputFile == -1)
                {
                    perror("open()");
                    fflush(stdout);
                    exit(1);
                }
                // redirect stdin to input file
                int result = dup2(inputFile, 0);
                if (result == -1)
                {
                    perror("dup2()");
                    fflush(stdout);
                    exit(2);
                }
            }

            // check for output file
            if (currCommand->outputFile != NULL)
            {
                int outputFile = open(currCommand->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if(outputFile == -1)
                {
                    perror("open()");
                    fflush(stdout);
                    exit(1);
                }
                // redirect stdout to output file
                int result = dup2(outputFile, 1);
                if(result == -1)
                {
                    perror("dup2");
                    fflush(stdout);
                    exit(2);
                }
            }         
            execvp(currCommand->arg[0], currCommand->arg);
            // exec only returns if there is an error
            perror("execvp()");
            fflush(stdout);
            exit(2);
            break;
        default:
        // In the parent process
        // Wait for child's termination

        // parent process wait for child to terminate
        childPid = waitpid(childPid, &childStatus, 0);
    
        // if successful exit, return exit status
        if (WIFEXITED(childStatus)) return WEXITSTATUS(childStatus); 
    
        // abnormal termintion, return termination signal
        else 
        {
            printf("terminated by signal %d\n", WTERMSIG(childStatus));
            fflush(stdout);
            return WTERMSIG(childStatus);
        }
    }  
}	

/* handles any background processes and returns the pid of child process */
pid_t backgroundProcess(struct commandLine *currCommand)
{
    int childStatus;
    int inputFile;
    int outputFile;
    pid_t spawnPid;

    //struct to set SIGTSTP to ignore
    struct sigaction SIGTSTP_child_action = {0};
    SIGTSTP_child_action.sa_handler = SIG_IGN;

    // Fork a new process
    pid_t childPid = fork();

    switch(childPid)
    {
        case -1:
            perror("fork()\n");
            fflush(stdout);
            exit(1);
            break;
        case 0:
            // In the child process
            
            //set SIGTSTP to ignore in child process
            sigaction(SIGTSTP, &SIGTSTP_child_action, NULL);
            
            // check for input file, if not specified, redirect to /dev/null
            if (currCommand->inputFile != NULL) inputFile = open(currCommand->inputFile, O_RDONLY);
            else inputFile = open("/dev/null", O_RDONLY);                
                
            if (inputFile == -1)
            {
                perror("sourcefile open()");
                fflush(stdout);
                exit(1);
            }
            // redirect stdin to input file
            int input = dup2(inputFile, 0);
            if (input == -1)
            {
                perror("dup2()");
                fflush(stdout);
                exit(2);
            }
            // check for output file
            if (currCommand->outputFile != NULL) outputFile =  open(currCommand->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            else outputFile = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0640);
            
               
            if(outputFile == -1)
            {
                perror("open()");
                fflush(stdout);
                exit(1);
            }
            //redirect stdout to output file
            int output = dup2(outputFile, 1);
            if(output == -1)
            {
                perror("dup2");
                fflush(stdout);
                exit(2);
            }
        
            execvp(currCommand->arg[0], currCommand->arg);
            // exec only returns if there is an error
            perror("execvp()");
            fflush(stdout);
            exit(2);
            break;
        
        default:
        // In the parent process
        
        // return pid of child
        printf("background pid is %d\n", childPid);
        fflush(stdout);
        return childPid;       
    }
}	

/* checks list of background processes for status */
void checkBackgroundProcesses(pid_t *processes)
{
    int childStatus;
    pid_t pid;
    
    int i = 0;
	while (processes[i] != 0 && processes[i] != -1) 
	{
		// check status of background processes
		pid = waitpid(processes[i], &childStatus, WNOHANG);

		// if process has terminated
		if (pid == processes[i])
		{
			// if successful exit, return exit status
            if (WIFEXITED(childStatus))
            {
                printf("background pid %d is done: exit value %d\n", pid, WEXITSTATUS(childStatus));
                fflush(stdout);
            } 
    
            // abnormal termintion, return termination signal
            else 
            {
                printf("background pid %d is done: terminated by signal %d\n", pid, WTERMSIG(childStatus)); 
                fflush(stdout);
            }

            // remove process from list
            processes[i] = -1;
		}
		i++;
	}
    return;
}

/* free memory from commandLine struct */
void freeCommand(struct commandLine *currCommand)
{
    int i = 0;
    while(currCommand->arg[i] != NULL)
    {
        free(currCommand->arg[i]);
        i++;
    }
    if(currCommand->inputFile != NULL) free(currCommand->inputFile);
    if(currCommand->outputFile != NULL) free(currCommand->outputFile);
}






    






