#include <stdbool.h>
#include <stdio.h>
#include "mimpi.h"


int main(int argc, char **argv)
{ 
    MIMPI_Init(false);

    int const world_rank = MIMPI_World_rank();

    int const tag = 17;

    char number;
    if (world_rank == 0)
    {
        number = 42;
        MIMPI_Send(&number, 1, 1, tag);
    }
    else if (world_rank == 1)
    {
        MIMPI_Recv(&number, 1, 0, tag);
        printf("Process 1 received number %d from process 0\n", number);
    }

    MIMPI_Finalize();
    return 0;
}















// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <unistd.h>
// #include <errno.h>
// #include <errno.h>
// #include <pthread.h>
// #include <signal.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <time.h>
// #include <unistd.h>

// const int rozmiar = 10;
// void dziecko_pisze() {
//     char wiadomosc1[rozmiar];
//     for(int i = 0; i < rozmiar; i++)
//         wiadomosc1[i] = 'a';

//     signal(SIGPIPE, SIG_IGN);

//     int a =  write(20, wiadomosc1, sizeof(wiadomosc1) - 1);
//     printf("%s", errno == EPIPE ? "EPIPE\n" : "OK\n");
//     //sleep(1);
//     printf("a: %d\n", a + 1);
// }


// void rodzic_czyta() {
    
//     close(20);
//     int ile = 0;
//     return;
//     while(ile < rozmiar - 1) {
//         char wiadomosc2[rozmiar];    
//         int b = read(20, wiadomosc2, sizeof(wiadomosc2));
//         printf("b: %d\n", b);
//         ile += b;
//     }
// }

// int main() {
//     int pipe_dsc[2];
//     pipe(pipe_dsc);
//     pid_t pid = fork();
//     if(pid == 0) {
//         // child
//         close(pipe_dsc[0]);
//         dup2(pipe_dsc[1], 20); // moje deskryptory to od 20 do 1023 wlacznie
//         dziecko_pisze();
//         close(pipe_dsc[1]);
//     }
//     else {
//         // parent
//         close(pipe_dsc[1]);
//         dup2(pipe_dsc[0], 20);
//         rodzic_czyta();
//         close(pipe_dsc[0]);
//         wait(NULL);
//     }

// }