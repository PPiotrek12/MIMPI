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

int main() {
    // Test case 1: Testing with integers

    MIMPI_Init(0);


    int rank = MIMPI_World_rank();
    
    if (rank == 0) {
        u_int8_t send_data[] = {1, 2, 3, 4, 5};
        u_int8_t count = sizeof(send_data) / sizeof(send_data[0]);
        u_int8_t *recv_data = malloc(count * sizeof(u_int8_t));
        
        
        MIMPI_Reduce(send_data, recv_data, count, MIMPI_PROD, 0);
        
        for( int i = 0; i < count; i++) {
            printf ("%d ", ((u_int8_t *)recv_data)[i]);
        }
        printf("\n");
        free(recv_data);
    }
    else {
        u_int8_t send_data[] = {4, 2, 4, 1, 9};
        u_int8_t count = sizeof(send_data) / sizeof(send_data[0]);

        u_int8_t *recv_data = malloc(count * sizeof(u_int8_t));

        MIMPI_Reduce(send_data, recv_data, count, MIMPI_PROD, 0);
        
        free(recv_data);
    }

    MIMPI_Finalize();
    return 0;
}

