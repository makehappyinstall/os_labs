#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

char *to_exec[] = {
        "/snap/bin/spotify",
        "/snap/bin/chromium google.com",
        "/usr/bin/firefox isu.ifmo.ru",
        "./useless first=1 second=2 qwe=67"
};

char *forbidden[] = {
        "/snap/bin/spotify"
};

int fill_argv(char *executable, char **argv) {
    char *token = strtok(executable, " ");
    if (token == NULL) {
        return 0;
    }
    if (forbidden[0] != NULL){
    	for (int i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); ++i) {
        	if (strcmp(token, forbidden[i]) == 0) {
        	    return 0;
        	}
    	}
    }
    if (access(token, F_OK) != 0) {
    	return 0;
    }
    int i = 0;
    do {
        argv[i++] = token;
        token = strtok(NULL, " ");
    } while (token != NULL);
    return 1;
}

int main() {
    printf("Executing main program, pid=%d\n", getpid());

    for (size_t i = 0; i < sizeof(to_exec) / sizeof(to_exec[0]); ++i) {
        char buff[200];
        strcpy(buff, to_exec[i]);

        char **argv = malloc(20 * sizeof(char *));
        if (!fill_argv(buff, argv)) {
            printf("{%s} executable is forbidden or can't be parsed, ignoring\n", to_exec[i]);
            continue;
        }

        if (fork() == 0) {
            printf("Executing %s in child process with pid=%d\n", to_exec[i], getpid());
            execv(argv[0], argv);
        }
    }

    sleep(30);

    return 0;
}
