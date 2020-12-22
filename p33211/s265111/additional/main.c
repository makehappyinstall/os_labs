#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include <stdbool.h>

char *forbidden[] = {
        "/usr/bin/ls",
        "/snap/bin/spotify"
};

bool is_same_files(char *executable, char *forbidden_exec) {
    struct stat stat1;
    struct stat stat2;
    stat(executable, &stat1);
    stat(forbidden_exec, &stat2);
    if (!S_ISREG(stat1.st_mode)) {
        //not a regular file
        return false;
    }
    return stat1.st_ino == stat2.st_ino;
}

bool is_valid_executable(char *executable) {
    if (executable == NULL) {
        return false;
    }
    if (access(executable, X_OK | F_OK) == -1) {
        return false;
    }
    if (forbidden[0] != NULL) {
        for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i) {
            if (is_same_files(executable, forbidden[i])) {
                return false;
            }
        }
    }
    return true;
}

bool fill_argv(char *executable, char **argv) {
    char *token = strtok(executable, " ");
    if (is_valid_executable(token)) {
        int i = 0;
        do {
            argv[i++] = token;
            token = strtok(NULL, " ");
        } while (token != NULL);
        return true;
    } else {
        return false;
    }
}

int main() {
    printf("Executing main program, pid=%d\n", getpid());

    char line[256];
    while (1) {
        puts("Enter a executable");
        if (fgets(line, sizeof line, stdin) == NULL) {
            continue;
        }
        line[strlen(line) - 1] = '\0';

        char **argv = malloc(20 * sizeof(char *));
        if (!fill_argv(line, argv)) {
            printf("{%s} executable is forbidden or can't be parsed, ignoring\n", line);
            continue;
        }

        if (fork() == 0) {
            printf("Executing %s in child process with pid=%d\n", line, getpid());
            if (execv(argv[0], argv) == -1) {
                printf("Something went wrong with %s executing, ignoring\n", line);
            }
        }
        sleep(1);
    }
    return 0;
}