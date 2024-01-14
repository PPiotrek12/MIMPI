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
    u_int8_t *data = malloc(10);
    for (int i = 0; i < 10; i++) {
        data[i] = i;
    }
    int dostaje = 0;
    if (rank == dostaje) {
        sleep(1);
        u_int8_t *recv = malloc(10);
        MIMPI_Reduce(data, recv, 10, MIMPI_MAX, dostaje);
        printf("Received: ");
        for (int i = 0; i < 10; i++) {
            printf("%d ", recv[i]);
        }
        printf("\n");
        free(recv);
    }
    else {
        MIMPI_Reduce(data, NULL, 10, MIMPI_MAX, dostaje);
    }
    free(data);
    MIMPI_Finalize();
}