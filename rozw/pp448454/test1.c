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
    int to_break = atoi(argv[1]);
    MIMPI_Init(false);
    if (MIMPI_World_rank() != to_break) {
        printf("%d\n", (MIMPI_Barrier() == MIMPI_ERROR_REMOTE_FINISHED));
    } else {
        printf("%d breaking barrier\n", MIMPI_World_rank());    
        sleep(1);
    }
    MIMPI_Finalize();
    return test_success();
}