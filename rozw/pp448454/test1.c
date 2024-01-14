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

    if (rank == 0) {
        char *data = "Hello world!";
        MIMPI_Bcast(data, 11, 0);
        printf("wyslalem\n");
    }
    else {
        char *data = malloc(11);
        MIMPI_Bcast(data, 11, 0);
        printf("Received: %s\n", data);
    }

    MIMPI_Finalize();
}