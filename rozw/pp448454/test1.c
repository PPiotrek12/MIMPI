#include <stdbool.h>
#include <stdio.h>
#include "mimpi.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>


int main(int argc, char **argv)
{ 
    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();
    int size = MIMPI_World_size();
    int const tag = 17;

    char number;

    char *a = (void *) malloc(1);
    *a = 'a';

    if (world_rank == 0)
    {
        MIMPI_Send(a, 1, 1, tag);
        printf("wyslalem\n");
        fflush(stdout);
    }
    else if (world_rank == 1)
    {
        MIMPI_Recv(&number, 1, 0, tag);
        printf("odebralem\n");
        fflush(stdout);
        sleep(1);
    }

    


    int b = MIMPI_Barrier();
    printf("%d\n", b);
    
    printf("koniec\n");
    MIMPI_Finalize();

    return 0;
}





