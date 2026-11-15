#include "shell.h"

int main() {
    char* cmdline;

    // Tell readline to call our function when Tab is pressed
    rl_attempted_completion_function = my_completion;

    while (1) {
        reap_jobs();                    // clean up finished background jobs (no zombies)

        // readline() prints the prompt, lets the user edit the line,
        // and returns a malloc'd string (NULL on Ctrl+D)
        cmdline = readline(PROMPT);
        if (cmdline == NULL)
            break;

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

        if (strncmp(cmdline, "if ", 3) == 0)
            run_if(cmdline);            // multi-line if-then-else-fi block
        else
            run_line(cmdline);          // split on ';' and run each command
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}