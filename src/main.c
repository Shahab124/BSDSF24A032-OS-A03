#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    // Tell readline to call our function when Tab is pressed
    rl_attempted_completion_function = my_completion;

    // readline() prints the prompt, lets the user edit the line,
    // and returns a malloc'd string (NULL on Ctrl+D)
    while ((cmdline = readline(PROMPT)) != NULL) {
        // !n : replace the line with command number n from history
        if (cmdline[0] == '!') {
            char* old = get_history(atoi(cmdline + 1));
            if (old == NULL) {
                printf("No such command in history\n");
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(old);
            printf("%s\n", cmdline);    // show what is being re-run
        }

        if (cmdline[0] != '\0') {       // don't save empty lines
            add_to_history(cmdline);    // our list (history, !n)
            add_history(cmdline);       // readline's list (Up/Down arrows)
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0)   // not a built-in?
                execute(arglist);               // then fork + exec it

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}