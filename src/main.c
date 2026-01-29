#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <sys/stat.h>

#define validCommands 6
#define PATH_MAX 1024
#define MAX_PIPE_CMDS 16
char* commands[validCommands] = {"exit", "echo", "type", "pwd", "cd", "history"};
static char** matches = NULL;
static int match_count = 0;
static int should_exit = 0, exit_status = 0;

void parseInput(char input[]);
int isValidBuiltin(char* command);
int isValidInPath(char* command, char** outputPath);
void handleExit(char* command, char* arguments[], int argumentCount);
void handleEcho(char* command, char* arguments[], int argumentCount);
void handleType(char* command, char* arguments[], int argumentCount);
void handlePwd(char* command, char* arguments[], int argumentCount, char** outputPath);
void handleCd(char* command, char* arguments[], int argumentCount);
void handleHistory(char* command, char* arguments[], int argumentCount);
void executableInPath(char* command, char* arguments[], int argumentCount,
                      char* inFile, char* outFile, int outAppend, char* errFile, int errAppend);
void redirectStds(char* arguments[], int* argumentCount,
                  char** inFile, char** outFile, int* outAppend, char** errFile, int* errAppend);
int split_pipeline(char* input, char* commands[], int max_cmds);
void execute_pipeline(char* input);
void read_history_file(const char* filename);
void write_history_file(const char* filename);
void append_history_file(const char* filename);

// Helper for builtins with redirection
typedef void (*builtin_func_t)(char*, char*[], int);

void run_with_redirection(
    builtin_func_t func,
    char* command,
    char* arguments[],
    int argumentCount,
    char* inFile,
    char* outFile,
    int outAppend,
    char* errFile,
    int errAppend
) {
    int saved_stdin = -1, saved_stdout = -1, saved_stderr = -1;
    FILE *fin = NULL, *fout = NULL, *ferr = NULL;

    if (inFile) {
        saved_stdin = dup(STDIN_FILENO);
        fin = fopen(inFile, "r");
        if (fin) dup2(fileno(fin), STDIN_FILENO);
    }
    if (outFile) {
        saved_stdout = dup(STDOUT_FILENO);
        fout = fopen(outFile, outAppend ? "a" : "w");
        if (fout) dup2(fileno(fout), STDOUT_FILENO);
    }
    if (errFile) {
        saved_stderr = dup(STDERR_FILENO);
        ferr = fopen(errFile, errAppend ? "a" : "w");
        if (ferr) dup2(fileno(ferr), STDERR_FILENO);
    }

    func(command, arguments, argumentCount);

    if (fin) { fflush(stdin); fclose(fin); }
    if (fout) { fflush(stdout); fclose(fout); }
    if (ferr) { fflush(stderr); fclose(ferr); }
    if (saved_stdin != -1) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }
    if (saved_stdout != -1) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
    if (saved_stderr != -1) { dup2(saved_stderr, STDERR_FILENO); close(saved_stderr); }
}

// Special wrapper for pwd (since it returns a string)
void handlePwdWrapper(char* command, char* arguments[], int argumentCount) {
    char* outputPath = NULL;
    handlePwd(command, arguments, argumentCount, &outputPath);
    printf("%s\n", outputPath);
    free(outputPath);
}

// char* command_generator(const char* text, int state) {
//     static int list_index, len;
//     static char* commands[] = {"exit", "echo", "type", "pwd", "cd", NULL};
//     if (!state) {
//         list_index = 0;
//         len = strlen(text);
//     }
//     while (commands[list_index]) {
//         char* name = commands[list_index];
//         list_index++;
//         if (strncmp(name, text, len) == 0) {
//             return strdup(name);
//         }
//     }
//     return NULL;
// }


void free_matches() {
    if (matches) {
        for (int i = 0; i < match_count; ++i) free(matches[i]);
        free(matches);
        matches = NULL;
        match_count = 0;
    }
}

char* command_generator(const char* text, int state) {
    static int list_index = 0;
    static int len = 0;

    if (!state) {
        // First call: build the matches array
        free_matches();
        list_index = 0;
        len = strlen(text);

        // 1. Add builtins
        int cap = 128;
        matches = malloc(cap * sizeof(char*));
        match_count = 0;
        for (int i = 0; i < validCommands; ++i) {
            if (strncmp(commands[i], text, len) == 0) {
                matches[match_count++] = strdup(commands[i]);
            }
        }

        // 2. Add executables from $PATH
        char* path = getenv("PATH");
        if (path) {
            char* path_copy = strdup(path);
            char* saveptr = NULL;
            char* dir = strtok_r(path_copy, ":", &saveptr);
            while (dir) {
                DIR* d = opendir(dir);
                if (d) {
                    struct dirent* entry;
                    while ((entry = readdir(d))) {
                        // Skip . and ..
                        if (entry->d_name[0] == '.') continue;
                        // Check if already in matches (avoid duplicates)
                        int already = 0;
                        for (int k = 0; k < match_count; ++k) {
                            if (strcmp(matches[k], entry->d_name) == 0) {
                                already = 1;
                                break;
                            }
                        }
                        if (already) continue;
                        // Check if executable
                        char fullpath[PATH_MAX];
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);
                        struct stat st;
                        if (stat(fullpath, &st) == 0 && (st.st_mode & S_IXUSR) && S_ISREG(st.st_mode)) {
                            if (strncmp(entry->d_name, text, len) == 0) {
                                if (match_count >= cap) {
                                    cap *= 2;
                                    matches = realloc(matches, cap * sizeof(char*));
                                }
                                matches[match_count++] = strdup(entry->d_name);
                            }
                        }
                    }
                    closedir(d);
                }
                dir = strtok_r(NULL, ":", &saveptr);
            }
            free(path_copy);
        }
    }

    // Return matches one by one
    if (list_index < match_count) {
        return strdup(matches[list_index++]);
    } else {
        free_matches();
        return NULL;
    }
}

char** my_completion(const char* text, int start, int end) {
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, command_generator);
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);

    // HISTFILE support: read on startup
    char* histfile = getenv("HISTFILE");
    if (histfile) {
        read_history_file(histfile);
    }

    rl_attempted_completion_function = my_completion;
    while(1) {
        char* input = readline("$ ");
        if (!input) break; // EOF (Ctrl-D)
        if (*input) add_history(input); // Add non-empty lines to history
        if (strchr(input, '|')) {
            execute_pipeline(input);
        } else {
            parseInput(input);
        }
        free(input);
        if(should_exit) break;
    }

    if (histfile) {
        write_history_file(histfile);
    }

    return 0;
}

void parseInput(char input[]) {
    char* command = (char*) malloc(101 * sizeof(char));
    char* argumentArray[100];
    int argumentCount = 0;
    int pointer = 0;

    // Parse command (first word)
    if(input[pointer] == '\'' || input[pointer] == '\"') {
        int start = 0;
        char quote = input[pointer++];

        while(input[pointer] != '\0' && input[pointer] != quote) {
            if (quote == '\"' && input[pointer] == '\\') {
                pointer++;
                if (input[pointer] != '\0') {
                    char escaped = input[pointer];
                    if (escaped == '"' || escaped == '\\' || escaped == '$' || escaped == ' ') {
                        command[start++] = input[pointer++];
                    } else {
                        command[start++] = '\\';
                        command[start++] = input[pointer++];
                    }
                }
            } else {
                command[start++] = input[pointer++];
            }
        }

        if(input[pointer] == quote) pointer++;
        command[start] = '\0';
    } else {
        int start = 0;
        while(input[pointer] != '\0' && !isspace(input[pointer])) {
            if(input[pointer] == '\\') pointer++;
            command[start++] = input[pointer++];
        }
        command[start] = '\0';
    }

    // Parse arguments (concatenate adjacent quoted/unquoted segments, keep previous handling)
    while (input[pointer] != '\0') {
        // Skip whitespace
        while (isspace(input[pointer])) pointer++;
        if (input[pointer] == '\0') break;

        char* argument = (char*) malloc(101 * sizeof(char));
        int start = 0;
        int segment_found = 0;

        // Keep accumulating adjacent quoted/unquoted segments into the same argument
        while (input[pointer] != '\0' && (!isspace(input[pointer]) || segment_found == 0)) {
            if (input[pointer] == '\'' || input[pointer] == '"') {
                char quote = input[pointer++];
                while (input[pointer] != '\0' && input[pointer] != quote) {
                    if (quote == '"' && input[pointer] == '\\') {
                        pointer++;
                        if (input[pointer] != '\0') {
                            char escaped = input[pointer];
                            if (escaped == '"' || escaped == '\\' || escaped == '$' || escaped == ' ') {
                                argument[start++] = input[pointer++];
                            } else {
                                argument[start++] = '\\';
                                argument[start++] = input[pointer++];
                            }
                        }
                    } else {
                        argument[start++] = input[pointer++];
                    }
                }
                if (input[pointer] == quote) pointer++;
                segment_found = 1;
            } else if (!isspace(input[pointer])) {
                if (input[pointer] == '\\') pointer++;
                if (input[pointer] != '\0' && !isspace(input[pointer]))
                    argument[start++] = input[pointer++];
                segment_found = 1;
            } else {
                // Only break if we've already found at least one segment for this argument
                break;
            }
        }
        argument[start] = '\0';
        argumentArray[argumentCount++] = argument;
    }


    // Redirection parsing
    char* inFile = NULL;
    char* outFile = NULL;
    char* errFile = NULL;
    int outAppend = 0, errAppend = 0;
    redirectStds(argumentArray, &argumentCount, &inFile, &outFile, &outAppend, &errFile, &errAppend);

    // Builtin or external
    if(strcmp(command, "exit") == 0) {
        run_with_redirection(handleExit, command, argumentArray, argumentCount, inFile, outFile, outAppend, errFile, errAppend);
    } else if(strcmp(command, "echo") == 0) {
        run_with_redirection(handleEcho, command, argumentArray, argumentCount, inFile, outFile, outAppend, errFile, errAppend);
    } else if(strcmp(command, "type") == 0) {
        run_with_redirection(handleType, command, argumentArray, argumentCount, inFile, outFile, outAppend, errFile, errAppend);
    } else if(strcmp(command, "pwd") == 0) {
        run_with_redirection(handlePwdWrapper, command, argumentArray, argumentCount, inFile, outFile, outAppend, errFile, errAppend);
    } else if(strcmp(command, "cd") == 0) {
        run_with_redirection(handleCd, command, argumentArray, argumentCount, inFile, outFile, outAppend, errFile, errAppend);
    } else if(strcmp(command, "history") == 0) {
        run_with_redirection(handleHistory, command, argumentArray, argumentCount, inFile, outFile, outAppend, errFile, errAppend);
    } else {
        executableInPath(command, argumentArray, argumentCount, inFile, outFile, outAppend, errFile, errAppend);
    }

    free(command);
    for(int i = 0 ;i < argumentCount; i++) {
        free(argumentArray[i]);
    }
    return;
}

void redirectStds(char* arguments[], int* argumentCount,
                  char** inFile, char** outFile, int* outAppend, char** errFile, int* errAppend) {
    *inFile = NULL;
    *outFile = NULL;
    *errFile = NULL;
    *outAppend = 0;
    *errAppend = 0;
    for(int i = 0; i < *argumentCount - 1; i++) {
        if(strcmp(arguments[i], "<") == 0) {
            *inFile = arguments[i+1];
            for(int j = i; j < *argumentCount - 2; j++) arguments[j] = arguments[j+2];
            *argumentCount -= 2;
            i--;
        } else if(strcmp(arguments[i], ">") == 0 || strcmp(arguments[i], "1>") == 0) {
            *outFile = arguments[i+1];
            *outAppend = 0;
            for(int j = i; j < *argumentCount - 2; j++) arguments[j] = arguments[j+2];
            *argumentCount -= 2;
            i--;
        } else if(strcmp(arguments[i], ">>") == 0 || strcmp(arguments[i], "1>>") == 0) {
            *outFile = arguments[i+1];
            *outAppend = 1;
            for(int j = i; j < *argumentCount - 2; j++) arguments[j] = arguments[j+2];
            *argumentCount -= 2;
            i--;
        } else if(strcmp(arguments[i], "2>") == 0) {
            *errFile = arguments[i+1];
            *errAppend = 0;
            for(int j = i; j < *argumentCount - 2; j++) arguments[j] = arguments[j+2];
            *argumentCount -= 2;
            i--;
        } else if(strcmp(arguments[i], "2>>") == 0) {
            *errFile = arguments[i+1];
            *errAppend = 1;
            for(int j = i; j < *argumentCount - 2; j++) arguments[j] = arguments[j+2];
            *argumentCount -= 2;
            i--;
        }
    }
}

int isValidBuiltin(char* command) {
    for(int i = 0; i < validCommands; i++) {
        if(strcmp(command, commands[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int isValidInPath(char* command, char** outputPath) {
    const char* path = getenv("PATH");
    if(command == NULL || path == NULL) return 0;
    char* duplicatePath = strdup(path);
    if(duplicatePath == NULL) {
        perror("strdup in isValidInPath\n");
        exit(1);
    }
    char* dir = strtok(duplicatePath, ":");
    while(dir != NULL) {
        char execPath[1024];
        sprintf(execPath,  "%s/%s", dir, command);
        if(access(execPath, X_OK) == 0) {
            *outputPath = strdup(execPath);
            free(duplicatePath);
            return 1;
        }
        dir = strtok(NULL, ":");
    }
    free(duplicatePath);
    return 0;
}

void handleExit(char* command, char* arguments[], int argumentCount) {
    if(argumentCount != 1) {
        printf("Usage: exit <number>\n");
        should_exit = 1;
        exit_status = 1;
    }
    exit_status = atoi(arguments[0]);
    should_exit = 1;

    return;
}

void handleEcho(char* command, char* arguments[], int argumentCount) {
    for(int i = 0; i < argumentCount; i++) {
        printf("%s", arguments[i]);
        if (i < argumentCount - 1) printf(" ");
    }
    printf("\n");
}

void handleType(char* command, char* arguments[], int argumentCount) {
    for (int i = 0; i < argumentCount; i++) {
        char* outputPath = NULL;
        if(isValidBuiltin(arguments[i]) == 1) {
            printf("%s is a shell builtin\n", arguments[i]);
        } else if(isValidInPath(arguments[i], &outputPath) == 1) {
            printf("%s is %s\n", arguments[i], outputPath);
            free(outputPath);
        } else {
            printf("%s: not found\n", arguments[i]);
        }
    }
}

void read_history_file(const char* filename) {
    // Don't print error for /dev/null or missing file
    if (!filename || strcmp(filename, "/dev/null") == 0) {
        read_history(filename); // still call it, but ignore result
        return;
    }

    if (read_history(filename) != 0) {
        fprintf(stderr, "Could not read history from %s\n", filename);
    }
}

void write_history_file(const char* filename) {
    if (write_history(filename) != 0) {
        fprintf(stderr, "Could not write history to %s\n", filename);
    }
}

void append_history_file(const char* filename) {
    // Appends all history entries to the file
    static int last_written = 0; // Number of entries already written
    FILE* f = fopen(filename, "a");
    if (!f) {
        fprintf(stderr, "Could not append history to %s\n", filename);
        return;
    }
    HIST_ENTRY **the_list = history_list();
    if (the_list) {
        for (int i = last_written; the_list[i]; i++) {
            fprintf(f, "%s\n", the_list[i]->line);
        }
        // Update last_written to the current history length
        last_written = history_length;
    }
    fclose(f);
}

void handleHistory(char* command, char* arguments[], int argumentCount) {
    // Support: history -r <file>, history -w <file>, history -a <file>
    if (argumentCount > 0 && arguments[0]) {
        if (strcmp(arguments[0], "-r") == 0 && argumentCount > 1) {
            read_history_file(arguments[1]);
            return;
        } else if (strcmp(arguments[0], "-w") == 0 && argumentCount > 1) {
            write_history_file(arguments[1]);
            return;
        } else if(strcmp(arguments[0], "-a") == 0 && argumentCount > 1) {
            append_history_file(arguments[1]);
            return;
        }
    }
    HIST_ENTRY **the_list = history_list();
    int num = history_length; // default: print all
    if (argumentCount > 0 && arguments[0]) {
        int n = atoi(arguments[0]);
        if (n > 0 && n < history_length)
            num = n;
    }
    int start = history_length - num;
    if (start < 0) start = 0;

    if (the_list) {
        for (int i = start; the_list[i]; i++) {
            printf("%d  %s\n", i + history_base, the_list[i]->line);
        }
    }
}


// void handleHistory(char* command, char* arguments[], int argumentCount) {
//     // Support: history -r <file>, history -w <file>
//     if (argumentCount > 0 && arguments[0]) {
//         if (strcmp(arguments[0], "-r") == 0 && argumentCount > 1) {
//             read_history_file(arguments[1]);
//             return;
//         } else if (strcmp(arguments[0], "-w") == 0 && argumentCount > 1) {
//             write_history_file(arguments[1]);
//             return;
//         }
//     }
//     int num = history_length; // default: print all
//     if (argumentCount > 0 && arguments[0]) {
//         int n = atoi(arguments[0]);
//         if (n > 0 && n < history_length)
//             num = n;
//     }
//     int start = history_length - num;
//     if (start < 0) start = 0;
//     for (int i = start; i < history_length; ++i) {
//         HIST_ENTRY *entry = history_get(i + history_base);
//         if (entry && entry->line)
//             printf("%d  %s\n", i + history_base, entry->line);
//     }
// }

void executableInPath(char* command, char* arguments[], int argumentCount,
                      char* inFile, char* outFile, int outAppend, char* errFile, int errAppend) {
    char* outputPath = NULL;
    if(isValidInPath(command, &outputPath) == 0) {
        printf("%s: command not found\n", command);
        return;
    }
    char* execArgs[argumentCount + 2];
    execArgs[0] = command;
    for (int i = 0; i < argumentCount; i++) {
        execArgs[i + 1] = arguments[i];
    }
    execArgs[argumentCount + 1] = NULL;

    pid_t pid = fork();
    if(pid == 0) {
        // Redirection
        if(inFile) {
            int fd = open(inFile, O_RDONLY);
            if(fd < 0) { perror("open input"); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if(outFile) {
            int fd = open(outFile, O_WRONLY | O_CREAT | (outAppend ? O_APPEND : O_TRUNC), 0644);
            if(fd < 0) { perror("open output"); exit(1); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if(errFile) {
            int fd = open(errFile, O_WRONLY | O_CREAT | (errAppend ? O_APPEND : O_TRUNC), 0644);
            if(fd < 0) { perror("open error"); exit(1); }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        execv(outputPath, execArgs);
        perror("execv in executablesInPath");
        exit(1);
    } else if(pid < 0) {
        perror("fork in executablesInPath");
    } else {
        wait(NULL);
    }
    free(outputPath);
}

void handlePwd(char* command, char* arguments[], int argumentCount, char** outputPath) {
    char currentWorkingDirectory[PATH_MAX];
    if(getcwd(currentWorkingDirectory, PATH_MAX) == NULL) {
        perror("Error with getcwd");
        exit(1);
    }
    *outputPath = strdup(currentWorkingDirectory);
    if(outputPath == NULL) {
        perror("strdup failed in pwd");
        exit(1);
    }
}

void handleCd(char* command, char* arguments[], int argumentCount) {
    const char* path = NULL;
    if(argumentCount == 0 || strcmp(arguments[0], "~") == 0) {
        path = getenv("HOME");
        if(path == NULL) {
            perror("HOME not set\n");
            exit(1);
        }
    } else if(arguments[0][0] == '~') {
        char* home = getenv("HOME");
        if(home == NULL) {
            perror("HOME not set\n");
            exit(1);
        }
        char extendedPath[PATH_MAX];
        snprintf(extendedPath, PATH_MAX, "%s/%s", home, arguments[0] + 1);
        path = extendedPath;
    } else {
        path = arguments[0];
    }
    if(chdir(path) != 0) {
        printf("cd: %s: No such file or directory\n", path);
    }
}

int split_pipeline(char* input, char* cmds[], int max_cmds) {
    int count = 0;
    char* saveptr = NULL;
    char* token = strtok_r(input, "|", &saveptr);
    while (token && count < max_cmds) {
        // Remove leading/trailing whitespace
        while (isspace(*token)) token++;
        char* end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) *end-- = '\0';
        cmds[count++] = strdup(token);
        token = strtok_r(NULL, "|", &saveptr);
    }
    return count;
}

void execute_pipeline(char* input) {
    char* cmds[MAX_PIPE_CMDS];
    int n_cmds = split_pipeline(input, cmds, MAX_PIPE_CMDS);

    int pipes[MAX_PIPE_CMDS-1][2];
    for (int i = 0; i < n_cmds-1; ++i) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    for (int i = 0; i < n_cmds; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // If not the first command, redirect stdin
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            // If not the last command, redirect stdout
            if (i < n_cmds-1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Close all pipe fds in child
            for (int j = 0; j < n_cmds-1; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // Now parse and execute cmds[i] as usual (parseInput or exec)
            parseInput(cmds[i]);
            exit(0); // Should not reach here if exec succeeds
        }
    }
    // Parent closes all pipe fds
    for (int i = 0; i < n_cmds-1; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    // Wait for all children
    for (int i = 0; i < n_cmds; ++i) {
        wait(NULL);
        free(cmds[i]);
    }
}
