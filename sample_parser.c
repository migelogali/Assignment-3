/**
 * A sample program for parsing a command line. If you find it useful,
 * feel free to adapt this code for Assignment 4.
 * Do fix memory leaks and any additional issues you find.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h> // for status
#include <unistd.h>  // for execv
#include <fcntl.h> // for open
#include <sys/types.h> //for pid_t
#include <signal.h> // for sigaction


#define INPUT_LENGTH 2048
#define MAX_ARGS 512

int fg_only_mode = 0;

struct command_line
{
	char *argv[MAX_ARGS + 1];
	int argc;
	char *input_file;
	char *output_file;
	bool is_bg;
};

void handle_SIGTSTP(int signal_number) {
    if (fg_only_mode == 0) {
		char* enter_fg = "\nEntering foreground-only mode (& is now ignored)\n: ";
        fg_only_mode = 1;
        write(STDOUT_FILENO, enter_fg, 50);
    } else {
		char* exit_fg  = "\nExiting foreground-only mode\n: ";
        fg_only_mode = 0;
        write(STDOUT_FILENO, exit_fg, 30);
    }
}

struct command_line *parse_input()
{
	char input[INPUT_LENGTH];
	struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

	// Display prompt and get input
	while(true) {
		printf(": ");
		fflush(stdout);
		fgets(input, INPUT_LENGTH, stdin);

		// Reprompt if first character given is # or blank line
        if (input[0] == '#' || input[0] == '\n') {
            continue;
        }
		break;
	}

	// Tokenize the input
	char *token = strtok(input, " \n");
	while(token) {
		if(!strcmp(token, "<")) {
			curr_command->input_file = strdup(strtok(NULL, " \n"));
		}
		else if(!strcmp(token, ">")) {
			curr_command->output_file = strdup(strtok(NULL, " \n"));
		}
		// regular argument
		else {
			curr_command->argv[curr_command->argc++] = strdup(token);
		}
		// go to next part of argument
		token = strtok(NULL, " \n");
	}

	// Check if needs to run in background if last argument is &
	if (curr_command->argc > 0) {
		if (strcmp(curr_command->argv[curr_command->argc - 1], "&") == 0) {
			if (fg_only_mode == 0) {
            	curr_command->is_bg = true;
			}
			// ignore & if in foreground
			else {
				curr_command->is_bg = false;
			}
			// don't need & argument anymore since taken care of
			free(curr_command->argv[curr_command->argc - 1]);
			curr_command->argc--;
		}
	}
	// replace with NULL in last argv for exec()
	curr_command->argv[curr_command->argc] = NULL;

	return curr_command;
}

// free all pointers for args, input, and output from struct, then free struct
void free_command_line(struct command_line *curr_command) {
	// protects against blank line
    if (curr_command == NULL) {
		return;
	}

    for (int i = 0; i < curr_command->argc; i++) {
        free(curr_command->argv[i]);
    }

    if (curr_command->input_file) {
        free(curr_command->input_file);
    }

    if (curr_command->output_file) {
        free(curr_command->output_file);
    }

    free(curr_command);
}

int main() {
	struct command_line *curr_command;
	// in case no foreground or signal set
	int status_val = 0;
	int childStatus;
	pid_t spawnPid;

	struct sigaction SIGINT_action = {0};
	// Fill out the SIGINT_action struct
	// Register SIG_IGN as the signal handler
	SIGINT_action.sa_handler = SIG_IGN;
	// Block all catchable signals while SIG_IGN is running
	sigfillset(&SIGINT_action.sa_mask);
	// No flags set
	SIGINT_action.sa_flags = 0;
	// Install our signal handler
  	sigaction(SIGINT, &SIGINT_action, NULL);

	// similarly with sigstp, but this includes a created signal handler
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	while(true)
	{
		curr_command = parse_input();
		// to avoid strcmp error if blank line entered
		if (curr_command->argc > 0) {
			if (strcmp(curr_command->argv[0], "exit") == 0) {
				while ((spawnPid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
					if (WIFEXITED(childStatus)) {
						printf("background pid %d is done: exit value %d\n", spawnPid, WEXITSTATUS(childStatus));
					}
					else {
						printf("background pid %d is done: terminated by signal %d\n", spawnPid, WTERMSIG(childStatus));
					}
					fflush(stdout);
				}

				free_command_line(curr_command);
    			return EXIT_SUCCESS;
			}
			else if (strcmp(curr_command->argv[0], "cd") == 0) {
				if (curr_command->argv[1] == NULL) {
					chdir(getenv("HOME"));
				}
				else {
					// value of 0 means successfully changed, per man pages
					if (chdir(curr_command->argv[1]) != 0) {
            			perror("cd");
					}
				}
				free_command_line(curr_command);
    			continue;
			}
			else if (strcmp(curr_command->argv[0], "status") == 0) {
				// copied from 'Process API - Monitoring Child Processes' exploration
				if (WIFEXITED(status_val)) {
					printf("exit value %d\n", WEXITSTATUS(status_val));
				}
				else {
					printf("terminated by signal %d\n", WTERMSIG(status_val));
				}
				free_command_line(curr_command);
				continue;
			}
			// adapted from 'Process API - Executing a New Program' exploration
			else {
				// Fork a new process
				spawnPid = fork();

				switch(spawnPid){
				case -1:
					perror("fork()\n");
					exit(1);
					break;
				case 0:
					// handle input file in child process first, then output file
					// adapted from 'Processes and I/O' exploration
					if (curr_command->input_file != NULL) {
						int sourceFD = open(curr_command->input_file, O_RDONLY);
						if (sourceFD == -1) { 
							perror("source open()"); 
							exit(1);
						}
						// Redirect stdin to source file
						int result = dup2(sourceFD, 0);
						if (result == -1) { 
							perror("source dup2()"); 
							exit(1);
						}
						// otherwise, input file successfully opened, so close it
						close(sourceFD);
					}
					if (curr_command->output_file != NULL) {
						int targetFD = open(curr_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if (targetFD == -1) { 
							perror("target open()"); 
							exit(1);
						}
						// Redirect stdout to target file
						int result = dup2(targetFD, 1);
						if (result == -1) { 
							perror("target dup2()"); 
							exit(1);
						}
						// otherwise, output file successfully opened, so close it
						close(targetFD);
					}

					// foreground child signal handling before execvp
					if (!curr_command->is_bg) {
						struct sigaction SIGINT_action = {0};
						SIGINT_action.sa_handler = SIG_DFL;
						sigfillset(&SIGINT_action.sa_mask);
						SIGINT_action.sa_flags = 0;
						sigaction(SIGINT, &SIGINT_action, NULL);
					}

					// The child process executes this branch
					// Replace the current program
					execvp(curr_command->argv[0], curr_command->argv);

					// exec only returns if there is an error
					perror("execvp");
					exit(2);
					break;
				// The parent process executes this branch; wait for child's termination if not background
				default:
					if (curr_command->is_bg) {
    					printf("background pid is %d\n", spawnPid);
						fflush(stdout);
					}
					// all parent has to do in the foreground is call waitpid(), per section 6 instructions
					else {
						waitpid(spawnPid, &childStatus, 0);

						// using write to avoid reentrancy, realized ctrl-c only has signal 2
						if (WIFSIGNALED(childStatus)) {
							char* message = "terminated by signal 2\n";
  							write(STDOUT_FILENO, message, 23);
						}

						// update status instead of exiting
						status_val = childStatus;
						break;
					}
				}
			}
		}

		free_command_line(curr_command);

		// report when each child process finishes before next command
		// -1  in argument means 'waitpid() will wait for any child process', per exploration
		// wnohang makes it non-blocking
		while ((spawnPid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
			if (WIFEXITED(childStatus)) {
				printf("background pid %d is done: exit value %d\n", spawnPid, WEXITSTATUS(childStatus));
			}
			else {
				printf("background pid %d is done: terminated by signal %d\n", spawnPid, WTERMSIG(childStatus));
			}
			fflush(stdout);
		}
	}
	return EXIT_SUCCESS;
}