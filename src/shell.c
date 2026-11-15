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

// Is c one of the special one-character operators?
int is_operator(char c) {
    return c == '<' || c == '>' || c == '|' || c == '&';
}

// Splits a command line into words.
//   ls -l > out.txt      ->  {"ls", "-l", ">", "out.txt", NULL}
//   grep "a b"|wc        ->  {"grep", "a b", "|", "wc", NULL}
// Operators < > | & always become their own word, even without spaces.
// Text inside "double quotes" stays one word and the quotes are removed.
char** tokenize(char* cmdline) {
    char** arglist = malloc(sizeof(char*) * (MAXARGS + 1));
    int argnum = 0;
    char* cp = cmdline;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;   // skip spaces
        if (*cp == '\0') break;                   // end of line

        char word[MAX_LEN];
        int len = 0;

        if (is_operator(*cp)) {                   // operator = one-letter word
            word[len++] = *cp++;
        } else {                                  // normal word: read until space or operator
            while (*cp != '\0' && *cp != ' ' && *cp != '\t' && !is_operator(*cp)) {
                if (*cp == '"') {                 // quoted part: copy everything up to the closing "
                    cp++;
                    while (*cp != '\0' && *cp != '"')
                        word[len++] = *cp++;
                    if (*cp == '"') cp++;         // skip the closing "
                } else {
                    word[len++] = *cp++;
                }
            }
        }
        word[len] = '\0';
        arglist[argnum++] = strdup(word);         // store a malloc'd copy
    }

    if (argnum == 0) {                            // empty line
        free(arglist);
        return NULL;
    }
    arglist[argnum] = NULL;                       // execvp needs a NULL at the end
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
        show_jobs();
        return 1;
    }
    return 0;
}

// Runs ONE command (no ;). Returns its exit status (0 = success).
int run_command(char* cmd) {
    char** arglist = tokenize(cmd);
    if (arglist == NULL)                 // empty command
        return 0;

    // Does the command end with "&"?  e.g. "sleep 10 &"
    int background = 0;
    int last = 0;
    while (arglist[last + 1] != NULL)    // find the last word
        last++;
    if (strcmp(arglist[last], "&") == 0) {
        background = 1;
        free(arglist[last]);
        arglist[last] = NULL;            // remove "&" from the command
    }

    int status = 0;
    if (arglist[0] == NULL)
        fprintf(stderr, "syntax error near &\n");
    else if (handle_builtin(arglist) == 0)   // not a built-in?
        status = execute(arglist, background);

    for (int i = 0; arglist[i] != NULL; i++) // free the words
        free(arglist[i]);
    free(arglist);
    return status;
}

// Runs a whole line, which may hold several commands separated by ';'.
// They run one after another; returns the status of the last one.
int run_line(char* line) {
    int status = 0;
    char* copy = strdup(line);           // strtok changes the string, so use a copy
    char* cmd = strtok(copy, ";");
    while (cmd != NULL) {
        status = run_command(cmd);       // waits for it (unless it has &)
        cmd = strtok(NULL, ";");
    }
    free(copy);
    return status;
}