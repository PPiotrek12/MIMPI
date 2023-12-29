// #include <stdbool.h>
// #include <stdio.h>
// #include "mimpi.h"
// #include <unistd.h>
// #include <stdlib.h>
// #include <string.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <errno.h>


// int main(int argc, char **argv)
// { 
//     MIMPI_Init(false);

//     int const world_rank = MIMPI_World_rank();
//     int size = MIMPI_World_size();
//     int const tag = 17;

//     char number;

//     char *a = (void *) malloc(1);
//     *a = 'a';

//     a = (void *) malloc(1);
//     *a = 'a';
//     // printf("XD\n");
    
//     for(int i = 0; i < 2; i++) {
//         // MIMPI_Barrier();;
//         for (int i = 0; i < size; i++) {
//             if (i == world_rank) continue;    
//             printf ("send: %d\n", MIMPI_Send(a, 1, i, -1));
//         }
//         //sleep(1);
//         char b;
//         for (int i = 0; i < size; i++) {
//             if (i == world_rank) continue;
//             printf("receive: %d\n", MIMPI_Recv(&b, 1, i, -1));
            
//         }
        
//     }

    
//     //printf("koniec\n");


//     // int b = MIMPI_Barrier();
//     // printf("%d\n", b);
    
//     MIMPI_Finalize();

//     return 0;
// }






#include <stdbool.h>
#include <stdio.h>
#include "mimpi.h"
//#include "mimpi_err.h"

int main(int argc, char **argv)
{
    MIMPI_Init(false);
    int const process_rank = MIMPI_World_rank();
    int const size_of_cluster = MIMPI_World_size();

    for (int i = 0; i < 2; i++)
    {
        if (i == process_rank)
        {
            printf("Hello World from process %d of %d\n", process_rank, size_of_cluster);
            fflush(stdout);
        }
        int a = MIMPI_Barrier();
        printf("%d\n", a);
        fflush(stdout);
        
    }
    //sleep(1);
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