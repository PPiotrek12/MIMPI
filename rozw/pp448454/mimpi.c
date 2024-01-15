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
int graph_up[20], graph_down[20][20];

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

void list_init() {
    head = (void *) malloc(sizeof(struct list_elem));
    tail = (void *) malloc(sizeof(struct list_elem));
    if (head == NULL || tail == NULL) ASSERT_SYS_OK(-1);
    head->next = tail;
    head->prev = NULL;
    tail->next = NULL;
    tail->prev = head;
}

void add_to_list(char *data, int count, int tag, int source) {
    struct message *msg = (void *) malloc(sizeof(struct message));
    if (msg == NULL) ASSERT_SYS_OK(-1);
    msg->tag = tag;
    msg->count = count;
    msg->data = (void *) malloc(count);
    if (msg->data == NULL) ASSERT_SYS_OK(-1);
    memcpy(msg->data, data, count);
    msg->source = source;

    struct list_elem *elem = (void *)malloc(sizeof(struct list_elem));
    if (elem == NULL) ASSERT_SYS_OK(-1);

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

void destroy_list() {
    ASSERT_ZERO(pthread_mutex_lock(&my_mutex));
    struct list_elem *elem = head->next;
    while(elem != tail) {
        struct list_elem *next = elem->next;
        remove_from_list(elem);
        elem = next;
    }
    free(head);
    free(tail);
    ASSERT_ZERO(pthread_mutex_unlock(&my_mutex));
}


// ====================================== RECEIVING MESSAGES ======================================
int read_whole_message(int fd, void *data, int count) {
    int read_bytes = 0;
    while (read_bytes < count) {
        int to_read = count - read_bytes;
        int read_now;
        int res = read_now = chrecv(fd, data + read_bytes, to_read);
        if (res == -1 && errno == EBADF) // Thread interrupted.
            return -1;
        else if (res == -1) // Other error.
            ASSERT_SYS_OK(-1);
        if (res == 0) // Process finished.
            return MIMPI_ERROR_REMOTE_FINISHED;
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
        if (ret != MIMPI_ERROR_REMOTE_FINISHED) { // If there is any data to read.
            if (read_whole_message(to_me[i], &tag, 4) == -1) break;
            if (count != 0) {
                data = (void *) malloc(count);
                if (data == NULL) ASSERT_SYS_OK(-1);
                if (read_whole_message(to_me[i], data, count) == -1) 
                    break;
            }
        }

        ASSERT_SYS_OK(pthread_mutex_lock(&my_mutex));
        if (ret == MIMPI_ERROR_REMOTE_FINISHED) { // Process from which we want to read finished.
            finished[i] = true;
            
            if (if_waiting_for_message && w_source == i) 
                ASSERT_SYS_OK(pthread_cond_signal(&waiting_for_message_cond));
            ASSERT_SYS_OK(pthread_mutex_unlock(&my_mutex));
            break;
        }
        add_to_list(data, count, tag, i);
        free(data);

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
    if (source >= n_processes || source < 0)
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
int write_whole_message(int fd, const void *data, long long count) {
    long long written_bytes = 0;
    while(written_bytes < count) {
        long long to_write = count - written_bytes;
        long long written_now;
        long long res = written_now = chsend(fd, data + written_bytes, to_write);
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
    if (destination >= n_processes || destination < 0)
        return MIMPI_ERROR_NO_SUCH_RANK;

    long long total_size = 4 + 4 + (long long)count;
    void *message = malloc(total_size);
    if (message == NULL)
        ASSERT_SYS_OK(-1);
    memcpy(message, &count, 4);
    memcpy(message + 4, &tag, 4);
    memcpy(message + 8, data, count);

    int res = write_whole_message(from_me[destination], message, total_size);
    free(message);
    if (res == MIMPI_ERROR_REMOTE_FINISHED)
        return MIMPI_ERROR_REMOTE_FINISHED;
    return MIMPI_SUCCESS;
}


// ======================================== INITIALIZATION ========================================
void set_descriptors() {
    char* rank_str = getenv("MIMPI_RANK");
    to_me = (void *) malloc(n_processes * sizeof(int));
    from_me = (void *) malloc(n_processes * sizeof(int));
    if (to_me == NULL || from_me == NULL) ASSERT_SYS_OK(-1);

    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        char i_str[12];
        sprintf(i_str, "%d", i);
        char name[100] = "MIMPI_";
        strcat(name, rank_str), strcat(name, "_TO_"), strcat(name, i_str), strcat(name, "_WRITE");
        char* val = getenv(name);
        from_me[i] = atoi(val);
        char name2[100] = "MIMPI_";
        strcat(name2, i_str), strcat(name2, "_TO_"), strcat(name2, rank_str), strcat(name2, "_READ");
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

void create_graph() {
    graph_up[0] = -1, graph_up[1] = 0, graph_up[2] = 0, graph_up[3] = 2;
    graph_up[4] = 0, graph_up[5] = 4, graph_up[6] = 4, graph_up[7] = 6;
    graph_up[8] = 0, graph_up[9] = 8, graph_up[10] = 8, graph_up[11] = 10;
    graph_up[12] = 8, graph_up[13] = 12, graph_up[14] = 12, graph_up[15] = 14;

    for(int i = 0; i < n_processes; i++) {
        for(int j = 0; j < n_processes; j++) {
            if (graph_up[i] == j)
                graph_down[j][i] = 1;
            else
                graph_down[i][j] = 0;
        }
    }
}

void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();
    rank = atoi(getenv("MIMPI_RANK"));
    n_processes = atoi(getenv("MIMPI_N"));  
    set_descriptors();
    create_graph();
    list_init();

    ASSERT_ZERO(pthread_mutex_init(&my_mutex, NULL));
    ASSERT_ZERO(pthread_cond_init(&waiting_for_message_cond, NULL));

    finished = (void *) malloc(n_processes * sizeof(bool));
    if (finished == NULL) ASSERT_SYS_OK(-1);
    for(int i = 0; i < n_processes; i++)
        finished[i] = false;

    example_message = (void *) malloc(1);
    if (example_message == NULL) ASSERT_SYS_OK(-1);
    *example_message = 'a';

    // Creating threads.
    threads = (void *) malloc(n_processes * sizeof(pthread_t));
    args = (void *) malloc(n_processes * sizeof(int));
    if (threads == NULL || args == NULL) ASSERT_SYS_OK(-1);
    for(int i = 0; i < n_processes; i++) {
        if(i == rank) continue;
        args[i] = i;
        ASSERT_SYS_OK(pthread_create(&threads[i], NULL, wait_for_messages, &args[i]));
    }
}


// ========================================= FINALIZATION =========================================
void free_memory() {
    free(threads);
    free(args);
    free(to_me);
    free(from_me);
    free(example_message);
    free(finished);
    destroy_list();
}

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
    free_memory();
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
    // Waiting for messages from processes below.
    counter++;
    char b;
    for (int i = 0; i < n_processes; i++) {
        if (graph_down[rank][i] == 0) continue;
        if (MIMPI_Recv(&b, 1, i, -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }

    // Sending message to process above.
    if (rank != 0)
        if (MIMPI_Send(example_message, 1, graph_up[rank], -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    
    // Waiting for message from process above.
    counter++;
    if (rank != 0)
        if (MIMPI_Recv(&b, 1, graph_up[rank], -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    
    // Sending message to processes below.
    for (int i = n_processes - 1; i >= 0; i--) {
        if (graph_down[rank][i] == 0) continue;
        if (MIMPI_Send(example_message, 1, i, -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }
    return MIMPI_SUCCESS;
}

int down(int x, int root) {
    return (x - root + n_processes) % n_processes;
}
int up(int x, int root) {
    return (x + root) % n_processes;
}

MIMPI_Retcode MIMPI_Bcast(void *data, int count, int root) {
    if (root >= n_processes || root < 0)
        return MIMPI_ERROR_NO_SUCH_RANK;

    // Waiting for messages from processes below.
    counter++;
    char b;
    for (int i = 0; i < n_processes; i++) {
        if (graph_down[down(rank, root)][down(i, root)] == 0) continue;
        if (MIMPI_Recv(&b, 1, i, -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }

    // Sending message to process above.
    if (down(rank, root) != 0) {
        int res = MIMPI_Send(example_message, 1, up(graph_up[down(rank, root)], root), -counter);
        if (res == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }
    // Waiting for message from process above.
    counter++;
    if (down(rank, root) != 0) { 
        int res = MIMPI_Recv(data, count, up(graph_up[down(rank, root)], root), -counter);
        if (res == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }
    
    // Sending message to processes below.
    for (int i = n_processes - 1; i >= 0; i--) {
        if (graph_down[down(rank, root)][down(i, root)] == 0) continue;
        if (MIMPI_Send(data, count, i, -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }
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
    if (root >= n_processes || root < 0)
        return MIMPI_ERROR_NO_SUCH_RANK;

    // Waiting for messages from processes below.
    counter++;
    void *b = (void *) malloc(count);
    if (b == NULL) ASSERT_SYS_OK(-1);
    memcpy(b, send_data, count);

    for (int i = 0; i < n_processes; i++) {
        if (graph_down[down(rank, root)][down(i, root)] == 0) continue;

        void *recv_data = (void *) malloc(count);
        if (recv_data == NULL) ASSERT_SYS_OK(-1);

        if (MIMPI_Recv(recv_data, count, i, -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
        for (int j = 0; j < count; j++)
            *(u_int8_t *)((u_int8_t *)b + j) = ope(op, ((u_int8_t *)recv_data)[j], ((u_int8_t *)b)[j]);
        free(recv_data);
    }

    // Sending message to process above.
    if (down(rank, root) != 0) {
        int res = MIMPI_Send(b, count, up(graph_up[down(rank, root)], root), -counter);
        if (res == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }
    else
        memcpy(recv_data, b, count);
    free(b);

    // Waiting for message from process above.
    counter++;
    if (rank != 0)
        if (MIMPI_Recv(&b, 1, graph_up[rank], -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    
    // Sending message to processes below.
    for (int i = n_processes - 1; i >= 0; i--) {
        if (graph_down[rank][i] == 0) continue;
        if (MIMPI_Send(example_message, 1, i, -counter) == MIMPI_ERROR_REMOTE_FINISHED)
            return MIMPI_ERROR_REMOTE_FINISHED;
    }
    return MIMPI_SUCCESS;
}