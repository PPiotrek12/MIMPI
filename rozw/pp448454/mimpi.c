#include <stdlib.h>
#include "stdio.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "string.h"
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
//#include "channel.h"

int rank, n_processes;
int *to_me, *from_me, *to_me_write;
pthread_mutex_t mutex; // Mutex for list.
pthread_mutex_t waiting_for_message; // Semaphore for waiting for message.
pthread_t *threads;

bool if_waiting_for_message = false;
int w_tag, w_count, w_source;

struct message {
    int tag;
    int count;
    char *data;
    int source;
};

struct list_elem {
    struct message *msg;
    struct list_elem *next;
    struct list_elem *prev;
} *head, *tail;



// ====================================== RECEIVIND MESSAGES ======================================
void add_to_list(char *data, int count, int tag, int source) {
    struct message *msg = malloc(sizeof(struct message));
    msg->tag = tag;
    msg->count = count;
    msg->data = data;
    msg->source = source;

    struct list_elem *elem = malloc(sizeof(struct list_elem));

    pthread_mutex_lock(&mutex);
    elem->msg = msg;
    elem->next = tail;
    elem->prev = tail->prev;
    tail->prev->next = elem;
    tail->prev = elem;
    pthread_mutex_unlock(&mutex);
}

void read_whole_message(int fd, void *data, int count) {
    int read_bytes = 0;
    while(read_bytes < count) {
        int to_read = count - read_bytes;
        int read_now = read(fd, data + read_bytes, to_read);
        read_bytes += read_now;
    }
}

void* wait_for_messages(void* arg) {
    int i = *(int*)arg;
    while(1) {
        int count, tag;
        read_whole_message(to_me[i], &count, 4);
        if (count == -1) // Interrupting thread.
            return NULL;
            
        read_whole_message(to_me[i], &tag, 4); // TODO: check if tag is -1.

        char *data = malloc(count);
        read_whole_message(to_me[i], data, count);
        add_to_list(data, count, tag, i);

        if(if_waiting_for_message && w_tag == tag && w_count == count && w_source == i)
            pthread_mutex_unlock(&waiting_for_message);
    }
    return NULL;
}

void remove_from_list(struct list_elem *elem) {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    free(elem->msg->data);
    free(elem->msg);
    free(elem);
}

// Returns 1 if found, 0 otherwise.
int search_for_message(void *data, int count, int source, int tag) {
    pthread_mutex_lock(&mutex);
    struct list_elem *elem = head->next;
    while(elem != tail) {
        if(elem->msg->count == count && elem->msg->tag == tag && elem->msg->source == source) {
            memcpy(data, elem->msg->data, count);
            remove_from_list(elem);
            pthread_mutex_unlock(&mutex);
            return true;
        }
        elem = elem->next;
    }
    pthread_mutex_unlock(&mutex);
    return false;
}

MIMPI_Retcode MIMPI_Recv(void *data, int count, int source, int tag) {
    if (search_for_message(data, count, source, tag)) {
        return MIMPI_SUCCESS;
    }
    else {
        while(1) {
            if_waiting_for_message = true;
            w_tag = tag;
            w_count = count;
            w_source = source;

            pthread_mutex_lock(&waiting_for_message);

            if_waiting_for_message = false;
            if (search_for_message(data, count, source, tag)) {
                return MIMPI_SUCCESS;
            }
        }
    }
}





// ======================================= SENDING MESSAGES =======================================
MIMPI_Retcode MIMPI_Send(void const *data, int count, int destination, int tag) {
    write(from_me[destination], &count, 4);
    write(from_me[destination], &tag, 4);
    write(from_me[destination], data, count);
    return MIMPI_SUCCESS;
}





// ======================================== INITIALIZATION ========================================
void set_descriptors() {
    char* rank_str = getenv("MIMPI_RANK");
    to_me = malloc(n_processes * sizeof(int));
    from_me = malloc(n_processes * sizeof(int));
    to_me_write = malloc(n_processes * sizeof(int));
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
    
        char name3[100] = "MIMPI_";
        strcat(name3, i_str);
        strcat(name3, "_TO_");
        strcat(name3, rank_str);
        strcat(name3, "_WRITE");
        val = getenv(name3);
        to_me_write[i] = atoi(val);
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
        mine[to_me_write[i]] = 1;
    }
    for(int i = 21; i < how_many; i++)
        if(!mine[i]) 
            close(i);
    
}

void MIMPI_Init(bool enable_deadlock_detection) {
    //channels_init();
    rank = atoi(getenv("MIMPI_RANK"));
    n_processes = atoi(getenv("MIMPI_N"));  
    set_descriptors();

    pthread_mutex_init(&mutex, NULL);

    // Creating list.
    head = malloc(sizeof(struct list_elem));
    tail = malloc(sizeof(struct list_elem));
    head->next = tail;
    head->prev = NULL;
    tail->next = NULL;
    tail->prev = head;

    // Creating threads.
    threads = malloc(n_processes * sizeof(pthread_t));
    int args[n_processes]; // TODO: git?
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        args[i] = i;
        pthread_create(&threads[i], NULL, wait_for_messages, &args[i]);
    }    
}




// ========================================= FINALIZATION =========================================
void MIMPI_Finalize() {
    // Interrupting threads.
    for (int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        int msg = -1;
        write(to_me_write[i], &msg, 4);
    }

    // Waiting for threads to finish.
    for(int i = 0; i < n_processes; i++) { 
        if(i == rank) continue;
        pthread_join(threads[i], NULL);
    }
    free(threads);

    // Closing mine descriptors.
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        close(to_me[i]);
        close(from_me[i]);
        close(to_me_write[i]);
    }
    free(to_me);
    free(from_me);

    // Closing list.
    pthread_mutex_lock(&mutex);
    struct list_elem *elem = head->next;
    while(elem != tail) {
        remove_from_list(elem);
        elem = elem->next;
    }
    free(head);
    free(tail);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_destroy(&mutex);

    // channels_finalize();

}

int MIMPI_World_size() {
    return n_processes;
}

int MIMPI_World_rank() {
    return rank;
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