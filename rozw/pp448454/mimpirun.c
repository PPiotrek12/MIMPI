#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "channel.h"
#include "mimpi_common.h"

void set_channel_descriptors(int n) {
    int counter = 21; // First descriptor to use.
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            char i_str[12], j_str[12], read_str[12], write_str[12];
            int read = counter++, write = counter++;

            sprintf(i_str, "%d", i);
            sprintf(j_str, "%d", j);
            sprintf(read_str, "%d", read);
            sprintf(write_str, "%d", write);

            char name[100] = "MIMPI_";
            strcat(name, i_str);
            strcat(name, "_TO_");
            strcat(name, j_str);
            strcat(name, "_READ");
            setenv(name, read_str, 1);

            char name2[100] = "MIMPI_";
            strcat(name2, i_str);
            strcat(name2, "_TO_");
            strcat(name2, j_str);
            strcat(name2, "_WRITE");
            setenv(name2, write_str, 1);

            int fd[2];
            ASSERT_SYS_OK(channel(fd));
            ASSERT_SYS_OK(dup2(fd[0], read));
            ASSERT_SYS_OK(close(fd[0]));
            ASSERT_SYS_OK(dup2(fd[1], write));
            ASSERT_SYS_OK(close(fd[1]));
        }
    }
    char counter_str[10];
    sprintf(counter_str, "%d", counter);
    setenv("MIMPI_DESCRIPTOR_COUNTER", counter_str, 1);
}

int run_processes(int n, char* path, int argc, char** argv) {
    for (int i = 0; i < n; i++) {
        char rank[12];
        sprintf(rank, "%d", i);
        setenv("MIMPI_RANK", rank, 1);
    
        pid_t pid;
        ASSERT_SYS_OK(pid = fork());
        if (pid == 0) {
            char* args[argc - 1];
            args[0] = path;
            for (int j = 3; j < argc; j++) {
                args[j - 2] = argv[j];
            }
            args[argc - 2] = NULL;
            ASSERT_SYS_OK(execvp(path, args));
            return 1;
        }
    }

    for (int i = 21; i < atoi(getenv("MIMPI_DESCRIPTOR_COUNTER")); i++)
        ASSERT_SYS_OK(close(i));

    // Wait for all child processes to finish.
    for (int i = 0; i < n; i++)
        ASSERT_SYS_OK(wait(NULL));
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) 
        return 1;
    int n = atoi(argv[1]);
    char* path = argv[2];
    setenv("MIMPI_N", argv[1], 1);
    set_channel_descriptors(n);
    if(run_processes(n, path, argc, argv))
        return 1;
    return 0;
}