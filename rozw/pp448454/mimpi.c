#include <stdlib.h>
#include "stdio.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "string.h"
//#include "channel.h"

int rank, n_processes;
int *to_me, *from_me;

void MIMPI_Init(bool enable_deadlock_detection) {
    //channels_init();
    char* rank_str = getenv("MIMPI_RANK");
    rank = atoi(rank_str);

    char* n_str = getenv("MIMPI_N");
    n_processes = atoi(n_str);


    to_me = malloc(n_processes * sizeof(int));
    from_me = malloc(n_processes * sizeof(int));
    
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        char i_str[10];
        sprintf(i_str, "%d", i);

        char name[100] = "MIMPI_";
        strcat(name, rank_str);
        strcat(name, "_TO_");
        strcat(name, i_str);
        char* xd = getenv(name);
        from_me[i] = atoi(xd);

        char name2[100] = "MIMPI_";
        strcat(name2, i_str);
        strcat(name2, "_TO_");
        strcat(name2, rank_str);
        xd = getenv(name2);
        to_me[i] = atoi(xd);
    }
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        printf("%d to %d: %d\n", rank, i, from_me[i]);
        printf("%d to %d: %d\n", i, rank, to_me[i]);
    }


}

// void MIMPI_Finalize() {
//     TODO

//     channels_finalize();
// }

// int MIMPI_World_size() {
//     TODO
// }

int MIMPI_World_rank() {
    return rank;
}

// MIMPI_Retcode MIMPI_Send(
//     void const *data,
//     int count,
//     int destination,
//     int tag
// ) {
//     TODO
// }

// MIMPI_Retcode MIMPI_Recv(
//     void *data,
//     int count,
//     int source,
//     int tag
// ) {
//     TODO
// }

// MIMPI_Retcode MIMPI_Barrier() {
//     TODO
// }

// MIMPI_Retcode MIMPI_Bcast(
//     void *data,
//     int count,
//     int root
// ) {
//     TODO
// }

// MIMPI_Retcode MIMPI_Reduce(
//     void const *send_data,
//     void *recv_data,
//     int count,
//     MIMPI_Op op,
//     int root
// ) {
//     TODO
// }