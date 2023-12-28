#include <stdlib.h>
#include "stdio.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "string.h"
#include <sys/types.h>
#include <unistd.h>
//#include "channel.h"

int rank, n_processes;
int *to_me, *from_me;


void set_descriptors() {
    char* rank_str = getenv("MIMPI_RANK");
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
        strcat(name, "_WRITE");
        char* val = getenv(name);
        from_me[i] = atoi(val);

        char name2[100] = "MIMPI_";
        strcat(name2, i_str);
        strcat(name2, "_TO_");
        strcat(name2, rank_str);
        strcat(name2, "_READ");
        val = getenv(name2);
        to_me[i] = atoi(val);
    }

    // Closing not mine descriptors.
    int how_many = atoi(getenv("MIMPI_DESCRIPTOR_COUNTER"));
    int mine[how_many];
    for(int i = 0; i < how_many; i++) 
        mine[i] = 0;
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        mine[to_me[i]] = 1;
        mine[from_me[i]] = 1;
    }
    for(int i = 21; i < how_many; i++)
        if(!mine[i]) 
            close(i);
}

void MIMPI_Init(bool enable_deadlock_detection) {
    //channels_init();
    char* rank_str = getenv("MIMPI_RANK");
    rank = atoi(rank_str);

    char* n_str = getenv("MIMPI_N");
    n_processes = atoi(n_str);

    set_descriptors();
}

void MIMPI_Finalize() {
    // Closing mine descriptors.
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        close(to_me[i]);
        close(from_me[i]);
    }

    free(to_me);
    free(from_me);

    // channels_finalize();
}

int MIMPI_World_size() {
    return n_processes;
}

int MIMPI_World_rank() {
    return rank;
}

MIMPI_Retcode MIMPI_Send(void const *data, int count, int destination, int tag) {
    write(from_me[destination], data, count);
    return MIMPI_SUCCESS;
}

MIMPI_Retcode MIMPI_Recv(void *data, int count, int source, int tag) {
    read(to_me[source], data, count);
    return MIMPI_SUCCESS;
}

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