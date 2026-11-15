#include "shell.h"

// ---------- Tab completion (readline) ----------
char* builtins[] = {"cd", "exit", "help", "jobs", "history", "set", NULL};

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
    if (strcmp(arglist[0], "set") == 0) {
        show_vars();
        return 1;
    }
    return 0;
}

// ---------- Shell variables (a linked list of name=value pairs) ----------
struct var {
    char* name;
    char* value;
    struct var* next;
};
struct var* var_list = NULL;          // first node of the list (empty at start)

// Create the variable, or change its value if it already exists.
void set_var(char* name, char* value) {
    for (struct var* v = var_list; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            free(v->value);
            v->value = strdup(value);
            return;
        }
    }
    struct var* v = malloc(sizeof(struct var));   // new node
    v->name = strdup(name);
    v->value = strdup(value);
    v->next = var_list;                // put it at the front of the list
    var_list = v;
}

// Returns the value of a variable, or NULL if it does not exist.
char* get_var(char* name) {
    for (struct var* v = var_list; v != NULL; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v->value;
    return NULL;
}

void show_vars() {
    for (struct var* v = var_list; v != NULL; v = v->next)
        printf("%s=%s\n", v->name, v->value);
}

// Replaces every word starting with $ by the variable's value.
// e.g. {"echo", "$MSG"} -> {"echo", "hello"}
void expand_variables(char** arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        if (arglist[i][0] != '$' || arglist[i][1] == '\0')
            continue;
        char* value = get_var(arglist[i] + 1);   // +1 skips the '$'
        if (value == NULL)
            value = getenv(arglist[i] + 1);      // also allow $HOME, $PATH, ...
        if (value == NULL)
            value = "";                          // unknown variable = empty
        free(arglist[i]);                        // free the old "$MSG" string
        arglist[i] = strdup(value);              // new memory for the value
    }
}

// Is this word an assignment like NAME=value?
// (has '=', and does not start with '=')
int is_assignment(char* word) {
    char* eq = strchr(word, '=');
    return eq != NULL && eq != word;
}

// Runs ONE command (no ;). Returns its exit status (0 = success).
int run_command(char* cmd) {
    char** arglist = tokenize(cmd);
    if (arglist == NULL)                 // empty command
        return 0;

    // VARNAME=value : store it, no fork needed
    if (is_assignment(arglist[0]) && arglist[1] == NULL) {
        char* eq = strchr(arglist[0], '=');
        *eq = '\0';                      // split "MSG=hi" into "MSG" and "hi"
        set_var(arglist[0], eq + 1);
        free(arglist[0]);
        free(arglist);
        return 0;
    }

    expand_variables(arglist);           // $NAME -> value, before running

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

// ---------- if-then-else-fi ----------
// first_line is the line the user typed, e.g. "if grep root /etc/passwd".
// We keep reading lines (prompt "> ") until "fi", putting the lines
// after "then" in one list and the lines after "else" in another.
// Then we run the condition and pick a list based on its exit status.
void run_if(char* first_line) {
    char* condition = first_line + 3;     // skip "if "
    char* then_lines[MAX_BLOCK];
    char* else_lines[MAX_BLOCK];
    int then_count = 0, else_count = 0;
    int part = 0;                         // 0 = before then, 1 = then-block, 2 = else-block
    int found_fi = 0;
    char* line;

    while ((line = readline("> ")) != NULL) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;   // ignore indentation

        if (strcmp(p, "fi") == 0) {
            found_fi = 1;
            free(line);
            break;
        } else if (strcmp(p, "then") == 0) {
            part = 1;
        } else if (strcmp(p, "else") == 0) {
            part = 2;
        } else if (part == 1 && then_count < MAX_BLOCK) {
            then_lines[then_count++] = strdup(p);
        } else if (part == 2 && else_count < MAX_BLOCK) {
            else_lines[else_count++] = strdup(p);
        } else if (part == 0) {
            printf("syntax error: expected 'then'\n");
        }
        free(line);
    }

    if (!found_fi || part == 0) {
        printf("syntax error: if needs 'then' and 'fi'\n");
    } else if (run_line(condition) == 0) {     // exit status 0 = success
        for (int i = 0; i < then_count; i++)
            run_line(then_lines[i]);
    } else {                                   // non-zero = failure
        for (int i = 0; i < else_count; i++)
            run_line(else_lines[i]);
    }

    for (int i = 0; i < then_count; i++) free(then_lines[i]);
    for (int i = 0; i < else_count; i++) free(else_lines[i]);
}