#include "shell.h"

// ---------- Tab completion (readline) ----------
char* builtins[] = {"cd", "exit", "help", "jobs", "history", NULL};

// readline calls this again and again for one Tab press.
// state == 0 on the first call: we collect all matches then.
// Each call returns one match; returning NULL means "no more".
char* command_generator(const char* text, int state) {
    static char matches[1000][256];
    static int count, index;

    if (state == 0) {
        count = 0;
        index = 0;
        int len = strlen(text);

        // 1) built-in names that start with text
        for (int i = 0; builtins[i] != NULL; i++)
            if (strncmp(builtins[i], text, len) == 0)
                strcpy(matches[count++], builtins[i]);

        // 2) program names in every PATH directory that start with text
        char* p = getenv("PATH");
        if (p == NULL) p = "/bin:/usr/bin";
        char* path = strdup(p);     // strtok changes the string, so copy it
        for (char* dir = strtok(path, ":"); dir != NULL; dir = strtok(NULL, ":")) {
            DIR* dp = opendir(dir);
            if (dp == NULL) continue;
            struct dirent* entry;
            while ((entry = readdir(dp)) != NULL && count < 1000)
                if (strncmp(entry->d_name, text, len) == 0)
                    snprintf(matches[count++], 256, "%s", entry->d_name);
            closedir(dp);
        }
        free(path);
    }

    if (index < count)
        return strdup(matches[index++]);   // readline frees this for us
    return NULL;
}

// Called by readline when Tab is pressed.
// start == 0 means we are on the first word -> complete command names.
// Returning NULL makes readline use its default: filename completion.
char** my_completion(const char* text, int start, int end) {
    if (start == 0)
        return rl_completion_matches(text, command_generator);
    return NULL;
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