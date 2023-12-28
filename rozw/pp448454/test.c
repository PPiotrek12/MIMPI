#include <stdlib.h>
#include "stdio.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "string.h"
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
//#include "channel.h"


int main(int argc, char **argv) {
    MIMPI_Init(0);
    int rank = MIMPI_World_rank();

    int size = 100;
    char send[size];
    for(int i = 0; i < size; i++)
        send[i] = 'a';

    if(rank == 0) { 
        //sleep(1);
        //MIMPI_Send(send, size, 1, 2137);

    }
    else if(rank == 1) {
        //sleep(3);
        char buf[size + 10];
    //    sleep(1);
        MIMPI_Recv(buf, size, 0, 2137);
        int ile = 0;
        for(int i = 0; i < size; i++)
            if(buf[i] == 'a')
                ile++;
        printf("ile: %d\n", ile);
        
        printf("buf: %s\n", buf);
    }

    sleep(2);
    MIMPI_Finalize();
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