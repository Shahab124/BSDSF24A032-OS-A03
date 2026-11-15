#include "shell.h"

// Runs in the CHILD only (after fork, before exec).
// Finds "< file" and "> file" in arglist, connects stdin/stdout to those
// files, and cuts them out of arglist so the program never sees them.
void setup_redirection(char** arglist) {
    int cut = -1;    // position of the first < or > (where the real command ends)

    for (int i = 0; arglist[i] != NULL; i++) {
        int is_in  = strcmp(arglist[i], "<") == 0;
        int is_out = strcmp(arglist[i], ">") == 0;
        if (!is_in && !is_out)
            continue;

        char* file = arglist[i + 1];
        if (file == NULL) {
            fprintf(stderr, "syntax error: missing file name\n");
            exit(1);
        }

        int fd;
        if (is_in)
            fd = open(file, O_RDONLY);                            // open file for reading
        else
            fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);  // create/empty file for writing
        if (fd < 0) {
            perror(file);
            exit(1);
        }

        if (is_in)
            dup2(fd, STDIN_FILENO);    // fd 0 (keyboard) now points to the file
        else
            dup2(fd, STDOUT_FILENO);   // fd 1 (screen) now points to the file
        close(fd);                     // the copy on 0/1 is enough, close the original

        if (cut == -1)
            cut = i;
        i++;                           // skip over the file name
    }

    if (cut != -1)
        arglist[cut] = NULL;           // "sort < a > b" becomes just "sort"
}

// Runs in the CHILD: set up redirection, then become the new program.
void run_child(char** arglist) {
    setup_redirection(arglist);
    if (arglist[0] == NULL) {          // e.g. "| wc" with nothing on the left
        fprintf(stderr, "syntax error: missing command\n");
        exit(1);
    }
    execvp(arglist[0], arglist);
    perror("Command not found");       // only reached if execvp failed
    exit(1);
}

// Runs a command (which may contain | pipes) and waits for it.
// Returns the command's exit status (0 = success).
int execute(char** arglist) {
    // Is there a pipe "|" in the command?
    int pipe_at = -1;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            pipe_at = i;
            break;
        }
    }

    // ----- No pipe: the normal fork-exec-wait -----
    if (pipe_at == -1) {
        int status;
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            return 1;
        }
        if (pid == 0)
            run_child(arglist);            // child never returns
        waitpid(pid, &status, 0);          // parent waits for the child
        return WEXITSTATUS(status);        // the child's exit code
    }

    // ----- Pipe: left | right -----
    if (arglist[pipe_at + 1] == NULL) {
        fprintf(stderr, "syntax error: missing command after |\n");
        return 1;
    }

    int fd[2];                             // fd[0] = read end, fd[1] = write end
    if (pipe(fd) == -1) {
        perror("pipe failed");
        return 1;
    }

    pid_t left = fork();
    if (left == 0) {                       // LEFT child (writer)
        arglist[pipe_at] = NULL;           // left command ends before "|"
        dup2(fd[1], STDOUT_FILENO);        // its output goes into the pipe
        close(fd[0]);                      // close both original pipe ends
        close(fd[1]);
        run_child(arglist);
    }

    pid_t right = fork();
    if (right == 0) {                      // RIGHT child (reader)
        dup2(fd[0], STDIN_FILENO);         // its input comes from the pipe
        close(fd[0]);
        close(fd[1]);
        // The right side may itself contain more pipes ("b | c"),
        // so we simply call execute() again on it.
        exit(execute(arglist + pipe_at + 1));
    }

    // PARENT: must close both ends, otherwise the reader never sees
    // end-of-file and waits forever.
    close(fd[0]);
    close(fd[1]);

    int status;
    waitpid(left, NULL, 0);
    waitpid(right, &status, 0);
    return WEXITSTATUS(status);
}
