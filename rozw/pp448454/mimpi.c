#include <stdlib.h>
#include "stdio.h"
#include "mimpi.h"
#include "mimpi_common.h"
#include "string.h"
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "channel.h"
#include <errno.h>

int rank, n_processes;
int *to_me, *from_me;
pthread_mutex_t my_mutex; 
pthread_cond_t waiting_for_message_cond; // Condition for waiting for message.
pthread_t *threads;
int *args;
bool *finished;
bool message_found = false;
char *example_message;

bool if_waiting_for_message = false;
int w_tag, w_count, w_source;


// ======================================= LIST OF MESSAGES =======================================
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

void add_to_list(char *data, int count, int tag, int source) {
    struct message *msg = (void *) malloc(sizeof(struct message));
    if (msg == NULL)
        ASSERT_SYS_OK(-1);
    msg->tag = tag;
    msg->count = count;
    msg->data = (void *) malloc(count);
    if (msg->data == NULL)
        ASSERT_SYS_OK(-1);
    memcpy(msg->data, data, count);
    msg->source = source;

    struct list_elem *elem = (void *)malloc(sizeof(struct list_elem));
    if (elem == NULL)
        ASSERT_SYS_OK(-1);

    elem->msg = msg;
    elem->next = tail;
    elem->prev = tail->prev;
    tail->prev->next = elem;
    tail->prev = elem;
}

void remove_from_list(struct list_elem *elem) {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    free(elem->msg->data);
    free(elem->msg);
    free(elem);
}




// ====================================== RECEIVING MESSAGES ======================================
int read_whole_message(int fd, void *data, int count) {
    int read_bytes = 0;
    while(read_bytes < count) {
        int to_read = count - read_bytes;
        int read_now;
        int res = read_now = chrecv(fd, data + read_bytes, to_read);
        if(res == -1 && errno == EBADF) // Thread interrupted.
            return -1;
        else if (res == -1)
            ASSERT_SYS_OK(-1);
        if (res == 0) {
            return -2;
        } // Process finished.

        read_bytes += read_now;
    }
    return 0;
}

void* wait_for_messages(void* arg) {
    int i = *(int*)arg;
    while(1) {
        int count, tag;
        int ret = read_whole_message(to_me[i], &count, 4);
        if (ret == -1) break;

        void *data = NULL;
        if (ret != -2) { // If there is any data to read.
            if (read_whole_message(to_me[i], &tag, 4) == -1) break;

            data = (void *) malloc(count);
            if (data == NULL)
                ASSERT_SYS_OK(-1);
            if (count != 0)
                if (read_whole_message(to_me[i], data, count) == -1) 
                    break;
        }

        ASSERT_SYS_OK(pthread_mutex_lock(&my_mutex));
        if (ret == -2) { // Process from which we want to read finished.
            finished[i] = true;
            
            if (if_waiting_for_message && w_source == i) 
                ASSERT_SYS_OK(pthread_cond_signal(&waiting_for_message_cond));
            ASSERT_SYS_OK(pthread_mutex_unlock(&my_mutex));
            break;
        }
        add_to_list(data, count, tag, i);

        if(if_waiting_for_message && w_count == count && w_source == i && 
            (w_tag == tag || w_tag == MIMPI_ANY_TAG)) 
            ASSERT_SYS_OK(pthread_cond_signal(&waiting_for_message_cond));

        ASSERT_SYS_OK(pthread_mutex_unlock(&my_mutex));
    }
    return NULL;
}




// Returns 1 and sets message_found = true if found, 0 otherwise.
int search_for_message(void *data, int count, int source, int tag) {
    struct list_elem *elem = head->next;
    while(elem != tail) {
        if(elem->msg->count == count && elem->msg->source == source && 
            (elem->msg->tag == tag || tag == MIMPI_ANY_TAG)) {
            memcpy(data, elem->msg->data, count);
            remove_from_list(elem);
            message_found = true;
            return true;
        }
        elem = elem->next;
    }
    return false;
}


MIMPI_Retcode MIMPI_Recv(void *data, int count, int source, int tag) {
    if (source == rank)
        return MIMPI_ERROR_ATTEMPTED_SELF_OP;
    if (source >= n_processes || source < 0) // TODO: czy to jest ok?
        return MIMPI_ERROR_NO_SUCH_RANK;

    ASSERT_SYS_OK(pthread_mutex_lock(&my_mutex));

    if (search_for_message(data, count, source, tag)) {
        ASSERT_ZERO(pthread_mutex_unlock(&my_mutex));
        return MIMPI_SUCCESS;
    }

    if (finished[source]) {
        ASSERT_ZERO(pthread_mutex_unlock(&my_mutex));
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    if_waiting_for_message = true;
    w_tag = tag;
    w_count = count;
    w_source = source;
    message_found = false;
    while(!search_for_message(data, count, source, tag) && !finished[source])
        ASSERT_ZERO(pthread_cond_wait(&waiting_for_message_cond, &my_mutex));

    if_waiting_for_message = false;

    if (!message_found) {
        ASSERT_ZERO(pthread_mutex_unlock(&my_mutex));
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    ASSERT_SYS_OK(pthread_mutex_unlock(&my_mutex));
    return MIMPI_SUCCESS;
}




// ======================================= SENDING MESSAGES =======================================
int write_whole_message(int fd, const void *data, int count) {
    int written_bytes = 0;
    while(written_bytes < count) {
        int to_write = count - written_bytes;
        int written_now;
        int res = written_now = chsend(fd, data + written_bytes, to_write);
        if(res == -1 && errno == EPIPE) // Process finished.
            return MIMPI_ERROR_REMOTE_FINISHED;
        else if (res == -1)
            ASSERT_SYS_OK(-1);
        written_bytes += written_now;
    }
    return 0;
}

MIMPI_Retcode MIMPI_Send(void const *data, int count, int destination, int tag) {
    if (destination == rank)
        return MIMPI_ERROR_ATTEMPTED_SELF_OP;
    if (destination >= n_processes || destination < 0) // TODO: czy to jest ok?
        return MIMPI_ERROR_NO_SUCH_RANK;
    
    if (write_whole_message(from_me[destination], &count, 4) == MIMPI_ERROR_REMOTE_FINISHED)
        return MIMPI_ERROR_REMOTE_FINISHED;
    if (write_whole_message(from_me[destination], &tag, 4) == MIMPI_ERROR_REMOTE_FINISHED)
        return MIMPI_ERROR_REMOTE_FINISHED;
    if (write_whole_message(from_me[destination], data, count) == MIMPI_ERROR_REMOTE_FINISHED)
        return MIMPI_ERROR_REMOTE_FINISHED;
    
    return MIMPI_SUCCESS;
}




// ======================================== INITIALIZATION ========================================
void set_descriptors() {
    char* rank_str = getenv("MIMPI_RANK");
    to_me = (void *) malloc(n_processes * sizeof(int));
    from_me = (void *) malloc(n_processes * sizeof(int));
    if (to_me == NULL || from_me == NULL)
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
            ASSERT_SYS_OK(close(i));
    
}

void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();
    rank = atoi(getenv("MIMPI_RANK"));
    n_processes = atoi(getenv("MIMPI_N"));  
    set_descriptors();

    ASSERT_ZERO(pthread_mutex_init(&my_mutex, NULL));
    ASSERT_ZERO(pthread_cond_init(&waiting_for_message_cond, NULL));

    // Creating list.
    head = (void *) malloc(sizeof(struct list_elem));
    tail = (void *) malloc(sizeof(struct list_elem));
    if (head == NULL || tail == NULL)
        ASSERT_SYS_OK(-1);
    head->next = tail;
    head->prev = NULL;
    tail->next = NULL;
    tail->prev = head;

    finished = (void *) malloc(n_processes * sizeof(bool));
    if (finished == NULL)
        ASSERT_SYS_OK(-1);
    for(int i = 0; i < n_processes; i++)
        finished[i] = false;


    example_message = (void *) malloc(1);
    if (example_message == NULL)
        ASSERT_SYS_OK(-1);
    *example_message = 'a';

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
    // Closing mine descriptors.
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        ASSERT_SYS_OK(close(to_me[i]));
        ASSERT_SYS_OK(close(from_me[i]));
    }

    // Waiting for threads to finish.
    for(int i = 0; i < n_processes; i++) { 
        if(i == rank) continue;
        ASSERT_SYS_OK(pthread_join(threads[i], NULL));
    }
    free(threads);
    free(args);
    free(to_me);
    free(from_me);
    free(example_message);

    // Closing list.
    ASSERT_ZERO(pthread_mutex_lock(&my_mutex));
    struct list_elem *elem = head->next;
    while(elem != tail) {
        remove_from_list(elem);
        elem = elem->next;
    }
    free(head);
    free(tail);
    ASSERT_ZERO(pthread_mutex_unlock(&my_mutex));

    ASSERT_ZERO(pthread_mutex_destroy(&my_mutex));
    ASSERT_ZERO(pthread_cond_destroy(&waiting_for_message_cond));

    channels_finalize();
}




// ======================================= WORLD FUNCTIONS ========================================
int MIMPI_World_size() {
    return n_processes;
}

int MIMPI_World_rank() {
    return rank;
}





// ======================================= GROUP FUNCTIONS ========================================

int counter = 0;
MIMPI_Retcode MIMPI_Barrier() {
    counter++;
    for (int i = 0; i < n_processes; i++) {
        if (i == rank) continue;
        if (MIMPI_Send(example_message, 1, i, -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }
    char b;
    for (int i = 0; i < n_processes; i++) {
        if (i == rank) continue;
        if (MIMPI_Recv(&b, 1, i, -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }
    return MIMPI_SUCCESS;
}

MIMPI_Retcode MIMPI_Bcast(void *data, int count, int root) {
    if (MIMPI_Barrier() == MIMPI_ERROR_REMOTE_FINISHED) 
        return MIMPI_ERROR_REMOTE_FINISHED;
    if (root >= n_processes || root < 0) // TODO: czy to jest ok?
        return MIMPI_ERROR_NO_SUCH_RANK;
    
    counter++;
    if (root == rank) {
        for (int i = 0; i < n_processes; i++) {
            if (i == rank) continue;
            MIMPI_Send(data, count, i, -counter);
        }
    }
    else 
        MIMPI_Recv(data, count, root, -counter);
    return MIMPI_SUCCESS;
}



u_int8_t ope(int op, u_int8_t a, u_int8_t b) {
    if (op == MIMPI_SUM) 
        return a + b;
    else if (op == MIMPI_MAX) {
        if (a < b) return b;
    }
    else if (op == MIMPI_MIN) {
        if (a > b) return b;
    }
    else if (op == MIMPI_PROD) return a * b;
    return a;
}


MIMPI_Retcode MIMPI_Reduce(void const *send_data, void *recv_data, int count, MIMPI_Op op, int root ) {
    if (MIMPI_Barrier() == MIMPI_ERROR_REMOTE_FINISHED) 
        return MIMPI_ERROR_REMOTE_FINISHED;

    if (root >= n_processes || root < 0) // TODO: czy to jest ok?
        return MIMPI_ERROR_NO_SUCH_RANK;

    counter++;
    if (rank == root) {
        memcpy(recv_data, send_data, count);
        for (int i = 0; i < n_processes; i++) {
            if (i == rank) continue;
            u_int8_t *data = (void *) malloc(count);
            if (data == NULL)
                ASSERT_SYS_OK(-1);
            MIMPI_Recv(data, count, i, -counter);
            for (int j = 0; j < count; j++)
                *(u_int8_t *)((u_int8_t *)recv_data + j) = ope(op, ((u_int8_t *)recv_data)[j], data[j]);
            free(data);
        }
    }
    else 
        MIMPI_Send(send_data, count, root, -counter);
    return MIMPI_SUCCESS;
}










// TODO : sprawdzic czy na pewno wszedzie sa asserty
// TODO : sprawdzic czy na pewno wszedzie sa free



/// japierdoleee sprawdzac wszedzie czy sa mutexy kurwa no