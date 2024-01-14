#include <stdbool.h>
#include <stdio.h>
#include "mimpi.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "mimpi.h"
#include <assert.h>
#include "examples/test.h"


int main(int argc, char **argv) {
    MIMPI_Init(false);
    int rank = MIMPI_World_rank();


    int const process_rank = MIMPI_World_rank();
    int const size_of_cluster = MIMPI_World_size();

    for (int i = 0; i < size_of_cluster; i++)
    {
        if (i == process_rank)
        {
            printf("Hello World from process %d of %d\n", process_rank, size_of_cluster);
            fflush(stdout);
        }
        MIMPI_Barrier();
    }

    MIMPI_Finalize();
}