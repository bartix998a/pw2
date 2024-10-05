/**
 * This file is for implementation of MIMPI library.
 * */
#define _GNU_SOURCE
// #define _UNIX03_THREADS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mman.h>
#include <pthread.h>
#include <inttypes.h>
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"

#define ATOMIC_SIZE 512

#define GOOD_RANK(x)                          \
    if (x == pid)                         \
    {                                         \
        return MIMPI_ERROR_ATTEMPTED_SELF_OP; \
    }                                         \
    else if (x < 0 || x >= world_size) \
    {                                         \
        return MIMPI_ERROR_NO_SUCH_RANK;      \
    }

#define GOOD_RANK_BCAST(x)               \
    if (x + 1 < 1 || x + 1 > world_size) \
    {                                    \
        return MIMPI_ERROR_NO_SUCH_RANK; \
    }

static int pid;
static int from_parent_fd;
static int to_parent_fd;
static int to_OS_public_fd;
static int to_OS_private_fd;
static int from_OS_fd;
static int to_rightson;
static int from_rightson;
static int to_leftson;
static int from_leftson;
static int bufferChannel;
static int world_size;
static bool deadlock_detection;
static bool deadlock = false;
pthread_t reciever;
pthread_mutex_t *buffer_protection;
pthread_mutex_t *await_correct_request;
static int request_count = -1;
static int request_source = -1;
static int request_tag = -1;
static buffer_t *recieve_buffer;
static bool any_finished = false;
static int from_OS_buffer;
static bool recieve_fail = false;
static int index_max;

void mimpi_send_request(int request)
{
    int message[2] = {pid, request};
    chsend(to_OS_public_fd, message, 2 * sizeof(int));
}

void generalized_send(int fd, const void *data, int count)
{
    for (int i = 0; i < count / ATOMIC_SIZE + (count % ATOMIC_SIZE == 0 ? 0 : 1); i++)
    {
        chsend(fd, data + ATOMIC_SIZE * i, count / ATOMIC_SIZE == i ? count % ATOMIC_SIZE : ATOMIC_SIZE); // samo sie zbuforuje (chyba)
    }
}

void generalized_recieve(int fd, void *data, int count)
{
    // for (int i = 0; i < count / ATOMIC_SIZE + (count % ATOMIC_SIZE == 0 ? 0 : 1); i++)
    // {
    //     chrecv(fd, data + ATOMIC_SIZE * i, count / ATOMIC_SIZE == i ? count % ATOMIC_SIZE : ATOMIC_SIZE); // samo sie zbuforuje (chyba)
    // }
    size_t passed = 0;
    while (passed != count)
    {
        passed += chrecv(fd, data + passed, count - passed);
    }
    
}

void init_static_variables()
{
    int result = 0;
    char *str = getenv("MIMPI_ID");
    sscanf(str, "%d", &result);
    pid = result;
    sscanf(getenv("MIMPI_from_OS"), "%d", &result);
    from_OS_fd = result;
    sscanf(getenv("MIMPI_to_OS_public"), "%d", &result);
    to_OS_public_fd = result;
    assert(to_OS_public_fd != -1);
    sscanf(getenv("MIMPI_to_OS_private"), "%d", &result);
    to_OS_private_fd = result;
    assert(to_OS_private_fd != -1);
    sscanf(getenv("MIMPI_from_parent"), "%d", &result);
    from_parent_fd = result;
    sscanf(getenv("MIMPI_to_right_son"), "%d", &result);
    to_rightson = result;
    sscanf(getenv("MIMPI_to_left_son"), "%d", &result);
    to_leftson = result;
    sscanf(getenv("MIMPI_from_buffer"), "%d", &result);
    bufferChannel = result;
    assert(bufferChannel != -1);
    world_size = atoi(getenv("MIMPI_world_size"));
    to_parent_fd = atoi(getenv("MIMPI_to_parent"));
    assert(to_parent_fd != -1);
    from_rightson = atoi(getenv("MIMPI_from_right_son"));
    from_leftson = atoi(getenv("MIMPI_from_left_son"));
    from_OS_buffer = atoi(getenv("MIMPI_from_OS_buffer"));
    index_max = atoi(getenv("MIMPI_index"));
}

void *recieve(void *arg)
{
    while (true)
    {
        int request[3];
        chrecv(from_OS_buffer, request, 3 * sizeof(int));
        
        pthread_mutex_lock(buffer_protection);
        if (request[2] == -2)
        {
            pthread_mutex_unlock(buffer_protection);
            break;
        } else if (request[2] == -1) {
            recieve_fail = true;
            pthread_mutex_unlock(await_correct_request);
            pthread_mutex_unlock(buffer_protection);
        } else if (request[2] == -3) {
            deadlock = true;
            request_count = -2;
            request_source = -2;
            request_tag = -2;
            pthread_mutex_unlock(await_correct_request);
            pthread_mutex_unlock(buffer_protection);
        } else {
            pthread_mutex_unlock(buffer_protection);
            buffer_t *element = malloc(sizeof(buffer_t));
            element->tag = request[2];
            element->source = request[1];
            element->count = request[0];
            element->buffor = malloc(request[0]);
            element->next = NULL;
            generalized_recieve(bufferChannel, element->buffor, element->count);
            pthread_mutex_lock(buffer_protection);
            push_back(recieve_buffer, element);

            if ((element->tag == request_tag || request_tag == 0) && element->count == request_count && element->source == request_source)
            {;
                
                request_tag = -1;
                request_count = -1;
                request_source = -1;
                pthread_mutex_unlock(await_correct_request);
            }
            pthread_mutex_unlock(buffer_protection);
            
        }
        
    }
    return NULL;
}

void initizlize_buffer()
{
    recieve_buffer = (buffer_t *)malloc(sizeof(buffer_t));
    recieve_buffer->tag = -1;
    recieve_buffer->count = 0;
    recieve_buffer->buffor = NULL;
    recieve_buffer->source = -1;
    recieve_buffer->next = NULL;
}

void initialize_mutexes()
{
    buffer_protection = malloc(sizeof(pthread_mutex_t));
    await_correct_request = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(buffer_protection, NULL);
    pthread_mutex_init(await_correct_request, NULL);
}

void MIMPI_Init(bool enable_deadlock_detection)
{
    deadlock_detection = enable_deadlock_detection;
    channels_init();
    init_static_variables();
    initizlize_buffer();
    initialize_mutexes();
    int *useless_shit = malloc(sizeof(int));
    *useless_shit = 1;
    ASSERT_ZERO(pthread_create(&reciever, NULL, recieve, NULL));
    free(useless_shit);
    pthread_mutex_lock(await_correct_request);
}

void close_fds(){
    for (int i = 20; i < index_max; i++) {
        ASSERT_SYS_OK(close(i));
    }
}

void MIMPI_Finalize()
{
    mimpi_send_request(MIMPI_FINALIZE);
    int *ret = NULL;
    char leftMPI = MIMPI_LEFT;
    pthread_join(reciever, (void **)&ret);
    remove_all(recieve_buffer);
    pthread_mutex_destroy(buffer_protection);
    pthread_mutex_destroy(await_correct_request);
    free(buffer_protection);
    free(await_correct_request);
    chsend(to_parent_fd, &leftMPI, sizeof(char));
    if (2 * (pid + 1) <= world_size)
    {
        chsend(to_leftson, &leftMPI, sizeof(char));
    }
    
    if (2 * (pid + 1) + 1 <= world_size)
    {
        chsend(to_rightson, &leftMPI, sizeof(char));
    }
    
    // send info to main thread and finish of reciever thread
    close_fds();
    channels_finalize();
}

int MIMPI_World_size()
{
    return world_size;
}

int MIMPI_World_rank()
{
    return pid;
}

MIMPI_Retcode MIMPI_Send(
    void const *data,
    int count,
    int destination,
    int tag)
{
    GOOD_RANK(destination);
    int request[5] = {pid, MIMPI_SEND, count, destination, tag};
    int dest_fd;
    chsend(to_OS_public_fd, request, 5 * sizeof(int));
    chrecv(from_OS_fd, &dest_fd, sizeof(int));
    if (dest_fd != 0)
    {
        generalized_send(dest_fd, data, count);
    }
    else
    {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    return MIMPI_SUCCESS;
}

MIMPI_Retcode MIMPI_Recv(
    void *data,
    int count,
    int source,
    int tag)
{
    GOOD_RANK(source);
    int response;
    int req[5] = {pid, deadlock_detection ? MIMPI_RECIEVE_DEADLOCK_DETECTION : MIMPI_RECIEVE, count, source, tag};

    chsend(to_OS_public_fd, req, 5 * sizeof(int));
    chrecv(from_OS_fd, &response, sizeof(int));
    if (response == ERROR)
    {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    if (deadlock_detection && response == DEADLOCK)
    {
        return MIMPI_ERROR_DEADLOCK_DETECTED; 
    }
    
    pthread_mutex_lock(buffer_protection);
    buffer_t *element = find_first(recieve_buffer, count, source, request_tag == -2 ? -2 : tag);
    if (element == NULL)
    {
        request_tag = request_tag == -2 ? -1 : tag;
        request_count = request_tag == -2 ? -1 : count;
        request_source = request_tag == -2 ? -1 : source;
        pthread_mutex_unlock(buffer_protection);
        pthread_mutex_lock(await_correct_request);
        pthread_mutex_lock(buffer_protection);
        if (recieve_fail)
        {
            recieve_fail = false;
            pthread_mutex_unlock(buffer_protection);
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
        if (deadlock_detection && deadlock)
        {
            deadlock = false;
            request_tag = -1;
            request_count = -1;
            request_source = -1;
            pthread_mutex_unlock(buffer_protection);
            return MIMPI_ERROR_DEADLOCK_DETECTED;
        }
        
        element = find_first(recieve_buffer, count, source, tag);
        
    }
    pthread_mutex_unlock(buffer_protection);
    memcpy(data, element->buffor, element->count);
    free(element->buffor);
    free(element);
    return MIMPI_SUCCESS;
}

bool MIMPI_BCAST_neighbour_left(char signal, int signal_producer)
{
    if (signal != MIMPI_LEFT)
    {
        return false;
    }
    any_finished = true;
    if (from_leftson != signal_producer)
    {
        chsend(to_leftson, &signal, sizeof(char));
    }
    if (from_rightson != signal_producer)
    {
        chsend(to_rightson, &signal, sizeof(char));
    }
    if (from_parent_fd != signal_producer)
    {
        chsend(to_parent_fd, &signal, sizeof(char));
    }
    return true;
}

MIMPI_Retcode MIMPI_Barrier()
{

    if (world_size == 1)
    {
        return MIMPI_SUCCESS;
    }
    

    if (any_finished)
    {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    char signal;
    if (2 * (pid + 1) <= world_size)
    {
        chrecv(from_leftson, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_leftson))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }
    if (2 * (pid + 1) + 1 <= world_size)
    {
        chrecv(from_rightson, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_rightson))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }

    if ((pid + 1) != 1)
    {
        signal = MIMPI_BARIER;
        chsend(to_parent_fd, &signal, 1);
        chrecv(from_parent_fd, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_parent_fd))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }

    if (2 * (pid + 1) <= world_size)
    {
        signal = MIMPI_BARIER;
        chsend(to_leftson, &signal, 1);
    }
    if (2 * (pid + 1) + 1 <= world_size)
    {
        signal = MIMPI_BARIER;
        chsend(to_rightson, &signal, 1);
    }

    return MIMPI_SUCCESS;
}

MIMPI_Retcode MIMPI_Bcast(
    void *data,
    int count,
    int root)
{
    GOOD_RANK_BCAST(root);
    char signal;
    char send_signal = pid == root ? MIMPI_BCAST_GOOD : MIMPI_BCAST_BAD;

    if (any_finished)
    {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    

    if (2 * (pid + 1) <= world_size)
    {
        chrecv(from_leftson, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_leftson))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
        if (signal == MIMPI_BCAST_GOOD)
        {
            generalized_recieve(from_leftson, data, count);
            send_signal = MIMPI_BCAST_GOOD;
        }
    }
    if (2 * (pid + 1) + 1 <= world_size)
    {
        chrecv(from_rightson, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_rightson))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
        if (signal == MIMPI_BCAST_GOOD)
        {
            generalized_recieve(from_rightson, data, count);
            send_signal = MIMPI_BCAST_GOOD;
        }
    }

    if ((pid + 1) != 1)
    {
        chsend(to_parent_fd, &send_signal, 1);
        if (send_signal == MIMPI_BCAST_GOOD)
        {
            generalized_send(to_parent_fd, data, count);
        }
        chrecv(from_parent_fd, &signal, 1);

        if (MIMPI_BCAST_neighbour_left(signal, from_parent_fd))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }

        generalized_recieve(from_parent_fd, data, count);
    }

    if (2 * (pid + 1) <= world_size)
    {
        chsend(to_leftson, &signal, 1);
        generalized_send(to_leftson, data, count);
    }
    if (2 * (pid + 1) + 1 <= world_size)
    {
        chsend(to_rightson, &signal, 1);
        generalized_send(to_rightson, data, count);
    }
    return MIMPI_SUCCESS;
}

uint8_t operation(uint8_t first, uint8_t second, MIMPI_Op op)
{
    switch (op)
    {
    case MIMPI_MAX:
        if (first > second)
        {
            return first;
        }
        else
        {
            return second;
        }
        break;
    case MIMPI_MIN:
        if (first < second)
        {
            return first;
        }
        else
        {
            return second;
        }
        break;
    case MIMPI_SUM:
        return first + second;
        break;
    case MIMPI_PROD:
        return first * second;
        break;
    }
    return 0;
}

// TODO: dziala za wolno
MIMPI_Retcode MIMPI_Reduce(
    void const *send_data,
    void *recv_data,
    int count,
    MIMPI_Op op,
    int root)
{
    GOOD_RANK_BCAST(root);
    char signal = MIMPI_BCAST_GOOD;
    char send_signal = MIMPI_BCAST_GOOD;
    uint8_t *left_recieve = NULL;
    uint8_t *right_recieve = NULL;
    uint8_t *send_buffer = malloc(count * sizeof(uint8_t));
    memcpy(send_buffer, send_data, count);

    if (2 * (pid + 1) <= world_size)
    {
        chrecv(from_leftson, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_leftson))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
        if (signal == MIMPI_BCAST_GOOD)
        {
            left_recieve = malloc(count * sizeof(uint8_t));
            generalized_recieve(from_leftson, left_recieve, count);
            send_signal = MIMPI_BCAST_GOOD;
        }
    }
    if (2 * (pid + 1) + 1 <= world_size)
    {
        chrecv(from_rightson, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_rightson))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
        if (signal == MIMPI_BCAST_GOOD)
        {
            right_recieve = malloc(count * sizeof(uint8_t));
            generalized_recieve(from_rightson, right_recieve, count);
            send_signal = MIMPI_BCAST_GOOD;
        }
    }

    for (int i = 0; i < count; i++)
    {
        if (left_recieve != NULL)
        {
            send_buffer[i] = operation(send_buffer[i], left_recieve[i], op);
        }

        if (right_recieve != NULL)
        {
            send_buffer[i] = operation(send_buffer[i], right_recieve[i], op);
        }
    }
    
    if ((pid + 1) != 1)
    {
        chsend(to_parent_fd, &send_signal, 1);
        if (send_signal == MIMPI_BCAST_GOOD)
        {
            generalized_send(to_parent_fd, send_buffer, count);
        }

        chrecv(from_parent_fd, &signal, 1);
        generalized_recieve(from_parent_fd, send_buffer, count);
        
        
        if (MIMPI_BCAST_neighbour_left(signal, from_parent_fd))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }

    if (pid == root)
    {
        memcpy(recv_data, send_buffer, count);
    }

    if (2 * (pid + 1) <= world_size)
    {
        chsend(to_leftson, &signal, 1);
        generalized_send(to_leftson, send_buffer, count);
    }
    if (2 * (pid + 1) + 1 <= world_size)
    {
        chsend(to_rightson, &signal, 1);
        generalized_send(to_rightson, send_buffer, count);
    }

    if (left_recieve != NULL)
    {
        free(left_recieve);
    }
    if (right_recieve != NULL)
    {
        free(right_recieve);
    }
    free(send_buffer);

    return MIMPI_SUCCESS;
}