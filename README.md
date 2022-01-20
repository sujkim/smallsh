# smallsh

A simple Unix shell written in C.

## Usage

: command [arg1 arg2 ...] [< input_file] [> output_file] [&]


## Built-in Commands
### exit
THe exit command exits the shell. Kills any other processes or jobs that shell has started before terminating itself.

### cd
The cd command changes the working directory of smallsh supports absolute and relative paths.

### status
The status command prints out either the exit status or the terminating signal of the last foreground process ran by the shell.

If this command is run before any foreground command is run, returns the exit status 0.

## Other Commands
Shell executes any commands other than the 3 built-in commands by using fork(), exec() and waitpid()
