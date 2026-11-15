#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 50
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20
#define MAX_JOBS 50
#define MAX_BLOCK 20

// Function prototypes
char** tokenize(char* cmdline);
int execute(char** arglist, int background);
void show_jobs();
void reap_jobs();
int run_command(char* cmd);
int run_line(char* line);
void run_if(char* first_line);
void set_var(char* name, char* value);
char* get_var(char* name);
void show_vars();
void expand_variables(char** arglist);
int handle_builtin(char** arglist);
void add_to_history(char* cmd);
void show_history();
char* get_history(int n);
char** my_completion(const char* text, int start, int end);

#endif // SHELL_H
