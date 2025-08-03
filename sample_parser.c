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

#define INPUT_LENGTH 2048
#define MAX_ARGS 512


struct command_line
{
	char *argv[MAX_ARGS + 1];
	int argc;
	char *input_file;
	char *output_file;
	bool is_bg;
};


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
			curr_command->is_bg = true;
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

	while(true)
	{
		curr_command = parse_input();
		// to avoid strcmp error if blank line entered
		if (curr_command->argc > 0) {
			if (strcmp(curr_command->argv[0], "exit") == 0) {
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
				int childStatus;
				// Fork a new process
				pid_t spawnPid = fork();

				switch(spawnPid){
				case -1:
					perror("fork()\n");
					exit(1);
					break;
				case 0:
					// handle input file in child process first, then output file
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

					// The child process executes this branch
					printf("CHILD(%d) running command %s\n", getpid(), curr_command->argv[0]);
					// Replace the current program
					execvp(curr_command->argv[0], curr_command->argv);

					// exec only returns if there is an error
					perror("execvp");
					exit(2);
					break;
				default:
					// The parent process executes this branch; wait for child's termination
					spawnPid = waitpid(spawnPid, &childStatus, 0);
					// update status instead of exiting
					status_val = childStatus;
					printf("PARENT(%d): child(%d) terminated.\n", getpid(), spawnPid);
					break;
				}
			}
		}
	}
	return EXIT_SUCCESS;
}