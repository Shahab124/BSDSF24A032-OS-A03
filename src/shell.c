#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

// ---------- Command history ----------
char* history[HISTORY_SIZE];   // circular array: the last 20 commands
int history_count = 0;         // total commands ever added

void add_to_history(char* cmd) {
    int slot = history_count % HISTORY_SIZE;  // wrap around when full
    free(history[slot]);                      // drop the oldest (free(NULL) is ok)
    history[slot] = strdup(cmd);              // store our own copy
    history_count++;
}

// First command number still stored in the array (1-based).
int history_first() {
    if (history_count > HISTORY_SIZE)
        return history_count - HISTORY_SIZE + 1;
    return 1;
}

void show_history() {
    for (int n = history_first(); n <= history_count; n++)
        printf("%4d  %s\n", n, history[(n - 1) % HISTORY_SIZE]);
}

// Returns command number n, or NULL if it is not in the history.
char* get_history(int n) {
    if (n < history_first() || n > history_count)
        return NULL;
    return history[(n - 1) % HISTORY_SIZE];
}

// Runs built-in commands inside the shell process itself (no fork).
// Returns 1 if the command was a built-in, 0 otherwise.
int handle_builtin(char** arglist) {
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Bye!\n");
        exit(0);
    }
    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL)
            fprintf(stderr, "cd: missing directory\n");
        else if (chdir(arglist[1]) != 0)
            perror("cd");
        return 1;
    }
    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>   change directory\n");
        printf("  exit       quit the shell\n");
        printf("  help       show this message\n");
        printf("  jobs       list background jobs\n");
        printf("  history    show last %d commands (!n re-runs one)\n", HISTORY_SIZE);
        return 1;
    }
    if (strcmp(arglist[0], "history") == 0) {
        show_history();
        return 1;
    }
    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }
    return 0;
}