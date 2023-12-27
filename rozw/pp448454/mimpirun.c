#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#include "mimpi_common.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Wrong number of arguments\n");
        return 1;
    }
    int n = atoi(argv[1]);
    char* path = argv[2];

    setenv("MIMPI_N", argv[1], 1);

    int counter = 21; // First descriptor to use.
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            char i_str[10], j_str[10], counter_str[10];
            sprintf(i_str, "%d", i);
            sprintf(j_str, "%d", j);
            sprintf(counter_str, "%d", counter++);

            char name[100] = "MIMPI_";
            strcat(name, i_str);
            strcat(name, "_TO_");
            strcat(name, j_str);

            setenv(name, counter_str, 1);
        }
    }

    for (int i = 0; i < n; i++) {
        char rank[10];
        sprintf(rank, "%d", i);
        setenv("MIMPI_RANK", rank, 1);

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