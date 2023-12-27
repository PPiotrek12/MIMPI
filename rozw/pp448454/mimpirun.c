#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "mimpi_common.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Wrong number of arguments\n");
        return 1;
    }
    int n = atoi(argv[1]);
    char* path = argv[2];

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            char* args[argc - 1];
            args[0] = path;
            for (int j = 3; j < argc; j++) {
                args[j - 2] = argv[j];
            }
            args[argc - 2] = NULL;
            execv(path, args);  // TODO : czy to jest ok - cos mialo sie znajdowac w PATH?
            printf("Failed to execute program\n");
            return 1;
        }
    }
    // Wait for all child processes to finish.
    for (int i = 0; i < n; i++) {
        ASSERT_SYS_OK(wait(NULL));
    }

    return 0;
}