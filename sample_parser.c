/**
 * A sample program for parsing a command line. If you find it useful,
 * feel free to adapt this code for Assignment 4.
 * Do fix memory leaks and any additional issues you find.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

int main()
{
	struct command_line *curr_command;

	while(true)
	{
		curr_command = parse_input();
		// to avoid strcmp error if blank line entered
		if (curr_command->argc > 0) {
			if (strcmp(curr_command->argv[0], "exit") == 0) {
				free_command_line(curr_command);
    			return EXIT_SUCCESS;
			}
		}
	}
	return EXIT_SUCCESS;
}