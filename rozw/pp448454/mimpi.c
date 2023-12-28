#include <stdlib.h>
#include "stdio.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "string.h"
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "channel.h"

int rank, n_processes;
int *to_me, *from_me, *to_me_write;
pthread_mutex_t list_mutex; // Mutex for list.
pthread_mutex_t waiting_for_message; // Semaphore for waiting for message.
pthread_t *threads;
int *args;

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



// ====================================== RECEIVING MESSAGES ======================================
void add_to_list(char *data, int count, int tag, int source) {
    struct message *msg = (void *) malloc(sizeof(struct message));
    if (msg == NULL)
        ASSERT_SYS_OK(-1);
    msg->tag = tag;
    msg->count = count;
    msg->data = data;
    msg->source = source;

    struct list_elem *elem = (void *)malloc(sizeof(struct list_elem));
    if (elem == NULL)
        ASSERT_SYS_OK(-1);

    ASSERT_ZERO(pthread_mutex_lock(&list_mutex));
    elem->msg = msg;
    elem->next = tail;
    elem->prev = tail->prev;
    tail->prev->next = elem;
    tail->prev = elem;
    ASSERT_ZERO(pthread_mutex_unlock(&list_mutex));
}

void read_whole_message(int fd, void *data, int count) {
    int read_bytes = 0;
    while(read_bytes < count) {
        int to_read = count - read_bytes;
        int read_now;
        ASSERT_SYS_OK(read_now = chrecv(fd, data + read_bytes, to_read));
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

        char *data = (void *) malloc(count);
        if (data == NULL)
            ASSERT_SYS_OK(-1);
            
        read_whole_message(to_me[i], data, count);
        add_to_list(data, count, tag, i);

        if(if_waiting_for_message && w_count == count && w_source == i && 
            (w_tag == tag || w_tag == MIMPI_ANY_TAG))
            ASSERT_ZERO(pthread_mutex_unlock(&waiting_for_message));
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
    ASSERT_ZERO(pthread_mutex_lock(&list_mutex));
    struct list_elem *elem = head->next;
    while(elem != tail) {
        if(elem->msg->count == count && elem->msg->source == source && 
            (elem->msg->tag == tag || tag == MIMPI_ANY_TAG)) {
            memcpy(data, elem->msg->data, count);
            remove_from_list(elem);
            ASSERT_ZERO(pthread_mutex_unlock(&list_mutex));
            return true;
        }
        elem = elem->next;
    }
    ASSERT_ZERO(pthread_mutex_unlock(&list_mutex));
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

            ASSERT_ZERO(pthread_mutex_lock(&waiting_for_message));

            if_waiting_for_message = false;
            if (search_for_message(data, count, source, tag)) {
                return MIMPI_SUCCESS;
            }
        }
    }
}





// ======================================= SENDING MESSAGES =======================================
MIMPI_Retcode MIMPI_Send(void const *data, int count, int destination, int tag) {
    ASSERT_SYS_OK(chsend(from_me[destination], &count, 4));
    ASSERT_SYS_OK(chsend(from_me[destination], &tag, 4));
    ASSERT_SYS_OK(chsend(from_me[destination], data, count));
    return MIMPI_SUCCESS;
}





// ======================================== INITIALIZATION ========================================
void set_descriptors() {
    char* rank_str = getenv("MIMPI_RANK");
    to_me = (void *) malloc(n_processes * sizeof(int));
    from_me = (void *) malloc(n_processes * sizeof(int));
    to_me_write = (void *) malloc(n_processes * sizeof(int));
    if (to_me == NULL || from_me == NULL || to_me_write == NULL)
        ASSERT_SYS_OK(-1);

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
            ASSERT_SYS_OK(close(i));
    
}

void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();
    rank = atoi(getenv("MIMPI_RANK"));
    n_processes = atoi(getenv("MIMPI_N"));  
    set_descriptors();

    ASSERT_ZERO(pthread_mutex_init(&list_mutex, NULL));

    // Creating list.
    head = (void *) malloc(sizeof(struct list_elem));
    tail = (void *) malloc(sizeof(struct list_elem));
    if (head == NULL || tail == NULL)
        ASSERT_SYS_OK(-1);
    head->next = tail;
    head->prev = NULL;
    tail->next = NULL;
    tail->prev = head;

    // Creating threads.
    threads = (void *) malloc(n_processes * sizeof(pthread_t));
    args = (void *) malloc(n_processes * sizeof(int));
    if (threads == NULL || args == NULL)
        ASSERT_SYS_OK(-1);
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        args[i] = i;
        ASSERT_SYS_OK(pthread_create(&threads[i], NULL, wait_for_messages, &args[i]));
    }
}




// ========================================= FINALIZATION =========================================
void MIMPI_Finalize() {
    // Interrupting threads.
    for (int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        int msg = -1;
        ASSERT_SYS_OK(chsend(to_me_write[i], &msg, 4));
    }

    // Waiting for threads to finish.
    for(int i = 0; i < n_processes; i++) { 
        if(i == rank) continue;
        ASSERT_SYS_OK(pthread_join(threads[i], NULL));
    }
    free(threads);
    free(args);

    // Closing mine descriptors.
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        ASSERT_SYS_OK(close(to_me[i]));
        ASSERT_SYS_OK(close(from_me[i]));
        ASSERT_SYS_OK(close(to_me_write[i]));
    }
    free(to_me);
    free(from_me);
    free(to_me_write);

    // Closing list.
    ASSERT_ZERO(pthread_mutex_lock(&list_mutex));
    struct list_elem *elem = head->next;
    while(elem != tail) {
        remove_from_list(elem);
        elem = elem->next;
    }
    free(head);
    free(tail);
    ASSERT_ZERO(pthread_mutex_unlock(&list_mutex));

    ASSERT_ZERO(pthread_mutex_destroy(&list_mutex));
    ASSERT_ZERO(pthread_mutex_destroy(&waiting_for_message));

    channels_finalize();
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



// TODO : pododawac asserty_sys_ok