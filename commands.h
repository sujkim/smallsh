#ifndef COMMANDS_H
#define COMMANDS_H

void handle_SIGTSTP(int signo);
char *expandCommand(char *ptr,char *orig, char *newCommandPrompt);

struct commandLine
{
    char *arg[512];
    char *inputFile;
    char *outputFile;
    bool bg;
}; 

struct commandLine *createCommand(char *commandPrompt);
void cd (struct commandLine *currCommand);
pid_t foregroundProcess(struct commandLine *currCommand);
pid_t backgroundProcess(struct commandLine *currCommand);
void checkBackgroundProcesses(pid_t *processes);
void freeCommand(struct commandLine *currCommand);

#endif