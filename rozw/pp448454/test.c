#include <stdlib.h>
#include "stdio.h"
#include "mimpi.h"

int main(int argc, char **argv) {
    MIMPI_Init(0);

    int rank = MIMPI_World_rank();
    if(rank == 0) {
        MIMPI_Send("xd", 2, 1, 0);
    }
    else if(rank == 1) {
        char buf[100];
        MIMPI_Recv(buf, 100, 0, 0);
        printf("buf: %s\n", buf);
    }


    MIMPI_Finalize();

}

















// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <unistd.h>

// const int rozmiar = 1000000;
// void dziecko_pisze() {
//     return;
//     char wiadomosc1[rozmiar];
    
//     for(int i = 0; i < rozmiar; i++)
//         wiadomosc1[i] = 'a';

//     int a =  write(20, wiadomosc1, sizeof(wiadomosc1) - 1);
//     //sleep(1);
//     printf("a: %d\n", a + 1);
// }


// void rodzic_czyta() {
//     int ile = 0;
//     char* xd = getenv("xd");
//     printf("xd: %s\n", xd);
//     return;
//     while(ile < rozmiar - 1) {
//         char wiadomosc2[rozmiar];    
//         int b = read(20, wiadomosc2, sizeof(wiadomosc2));
//         printf("b: %d\n", b);
//         ile += b;
//     }
// }

// int main() {
//     setenv("xd", "1000", 1);
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