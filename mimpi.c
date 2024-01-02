/**
 * This file is for implementation of MIMPI library.
 * */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/mman.h>
#include <pthread.h>
#include <inttypes.h>
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"

#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif

#define GOOD_RANK(x)                          \
    if (x == pid)                             \
    {                                         \
        return MIMPI_ERROR_ATTEMPTED_SELF_OP; \
    }                                         \
    else if (x < 1 || x > world_size)         \
    {                                         \
        return MIMPI_ERROR_NO_SUCH_RANK;      \
    }

#define GOOD_RANK_BCAST(x)               \
    if (x < 1 || x > world_size)         \
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
static pthread_t reciever;
pthread_mutex_t *buffer_protection;
pthread_mutex_t *await_correct_request;
static int request_count = -1;
static int request_source = -1;
static int request_tag = -1;
static buffer_t *recieve_buffer;
static bool any_finished = false;
static int from_OS_buffer;

void push_back(buffer_t *list, buffer_t *element)
{
    while (list->next != NULL)
    {
        list = list->next;
    }
    list->next = element;
    element->next = NULL;
}

buffer_t *find_first(buffer_t *list, int count, int source, int tag)
{
    while (list->next != NULL)
    {
        buffer_t *prev = list;
        list = list->next;
        if ((tag == 0 ? true : (list->tag == tag)) && list->source == source)
        {
            prev->next = list->next;
            return list;
        }
    }
}

void remove_element(buffer_t *list, buffer_t *element)
{
    while (list->next != NULL)
    {
        buffer_t *prev = list;
        list = list->next;
        if (list == element)
        {
            prev->next = list->next;
            list = element->next;
            element->next = NULL;
            return;
        }
    }
}

void remove_all(buffer_t *list)
{
    buffer_t *prev = list;
    while (list->next != NULL)
    {
        list = prev->next;
        free(list->buffor);
        prev->next = list->next;
        free(list);
    }
    free(prev);
}

void mimpi_send_request(int request)
{
    int message[2] = {pid, request};
    chsend(to_OS_public_fd, message, 2 * sizeof(int));
}

void generalized_send(int fd, void *data, int count)
{
    for (int i = 0; i < count / PIPE_BUF + (count % PIPE_BUF == 0 ? 0 : 1); i++)
    {
        chsend(fd, recieve_buffer + PIPE_BUF * i, count / PIPE_BUF == i ? count % PIPE_BUF : PIPE_BUF); // samo sie zbuforuje (chyba)
    }
}

void generalized_recieve(int fd, void *data, int count)
{
    for (int i = 0; i < count / PIPE_BUF + (count % PIPE_BUF == 0 ? 0 : 1); i++)
    {
        chrecv(fd, recieve_buffer + PIPE_BUF * i, count / PIPE_BUF == i ? count % PIPE_BUF : PIPE_BUF); // samo sie zbuforuje (chyba)
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
    sscanf(getenv("MIMPI_to_OS_private"), "%d", &result);
    to_OS_private_fd = result;
    sscanf(getenv("MIMPI_from_parent"), "%d", &result);
    from_parent_fd = result;
    sscanf(getenv("MIMPI_to_right_son"), "%d", &result);
    to_rightson = result;
    sscanf(getenv("MIMPI_to_left_son"), "%d", &result);
    to_leftson = result;
    sscanf(getenv("MIMPI_from_buffer"), "%d", &result);
    bufferChannel = result;
    recieve_buffer = malloc(sizeof(buffer_t));
    recieve_buffer->tag = -1;
    world_size = atoi(getenv("MIMPI_world_size"));
    to_parent_fd = atoi(getenv("MIMPI_to_parent"));
    from_rightson = atoi(getenv("MIMPI_from_right_son"));
    from_leftson = atoi(getenv("MIMPI_from_left_son"));
    from_OS_buffer = atoi(getenv("MIMPI_from_OS_buffer"));
}

void *recieve(void *arg)
{
    pthread_mutex_lock(buffer_protection);
    while (true)
    {
        pthread_mutex_unlock(buffer_protection);
        int request[3];
        chrecv(from_OS_buffer, request, 3 * sizeof(int)); // main przesyla

        if (request[2] == 0)
        {
            break;
        }

        buffer_t *element = malloc(sizeof(buffer_t));
        element->tag = request[2];
        element->count = request[0];
        element->buffor = malloc(request[0]);
        element->next = NULL;
        // for (int i = 0; i < element->count / PIPE_BUF + (element->count % PIPE_BUF == 0 ? 0 : 1); i++)
        // {
        //     chrecv(bufferChannel, element->buffor + PIPE_BUF * i, element->count / PIPE_BUF == i ? element->count % PIPE_BUF : PIPE_BUF); // samo sie zbuforuje (chyba)
        // }
        generalized_recieve(bufferChannel, element->buffor, element->count);
        pthread_mutex_lock(buffer_protection);
        push_back(recieve_buffer, element);
        push_back(recieve_buffer, element);
        if (element->tag == request_tag && element->tag == request_tag && element->source == request_source)
        {
            request_tag = -1;
            request_count = -1;
            request_source = -1;
            pthread_mutex_unlock(buffer_protection);
            pthread_mutex_unlock(await_correct_request);
        }
        else
        {
            pthread_mutex_unlock(buffer_protection);
        }
        pthread_mutex_lock(buffer_protection);
    }
}

void initizlize_buffer()
{
    recieve_buffer = (buffer_t *)malloc(sizeof(buffer_t));
    recieve_buffer->tag = -1;
    recieve_buffer->count = 0;
    recieve_buffer->buffor = NULL;
    recieve_buffer->source = -1;
}

void initialize_mutexes()
{
    pthread_mutexattr_t at;
    pthread_mutexattr_init(&at);
    buffer_protection = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(buffer_protection, &at);
    pthread_mutex_init(await_correct_request, &at);
    pthread_attr_destroy(&at);
}

void MIMPI_Init(bool enable_deadlock_detection)
{
    channels_init();
    init_static_variables();
    initizlize_buffer();

    pthread_attr_t a;
    ASSERT_ZERO(pthread_attr_init(&a));
    ASSERT_ZERO(pthread_attr_setdetachstate(&a, PTHREAD_CREATE_JOINABLE));
    pthread_create(&reciever, &a, recieve, NULL);
}

void MIMPI_Finalize()
{
    mimpi_send_request(MIMPI_FINALIZE);
    void *ret;
    char leftMPI = MIMPI_LEFT;
    pthread_join(&reciever, ret);
    remove_all(recieve_buffer);
    pthread_mutex_destroy(buffer_protection);
    pthread_attr_destroy(await_correct_request);
    chsend(to_parent_fd, &leftMPI, sizeof(char));
    chsend(to_leftson, &leftMPI, sizeof(char));
    chsend(to_rightson, &leftMPI, sizeof(char));
    // send info to main thread and finish of reciever thread
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
        // for (int i = 0; i < count / PIPE_BUF + (count % PIPE_BUF == 0 ? 0 : 1); i++)
        // {
        //     chsend(dest_fd, data + PIPE_BUF * i, count / PIPE_BUF == i ? count % PIPE_BUF : PIPE_BUF); // samo sie zbuforuje (chyba)
        // }
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
    mimpi_send_request(MIMPI_RECIEVE);
    chrecv(from_OS_fd, &response, sizeof(int));
    if (response == ERROR)
    {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    pthread_mutex_lock(buffer_protection);
    buffer_t *element = find_first(recieve_buffer, count, source, tag);
    if (element == NULL)
    {
        request_tag = tag;
        request_count = count;
        request_source = source;
        pthread_mutex_unlock(buffer_protection);
        pthread_mutex_lock(await_correct_request); // zly typ mutexa mamy problem
    }
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
    if (any_finished)
    {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }

    char signal;
    if (2 * pid <= world_size)
    {
        chrecv(from_leftson, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_leftson))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }
    if (2 * pid + 1 <= world_size)
    {
        chrecv(from_rightson, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_rightson))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }

    if (pid != 1)
    {
        signal = MIMPI_BARIER;
        chsend(to_parent_fd, &signal, 1);
        chrecv(from_parent_fd, &signal, 1);
        if (MIMPI_BCAST_neighbour_left(signal, from_parent_fd))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }

    if (2 * pid <= world_size)
    {
        signal = MIMPI_BARIER;
        chsend(to_leftson, &signal, 1);
    }
    if (2 * pid + 1 <= world_size)
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

    if (2 * pid <= world_size)
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
    if (2 * pid + 1 <= world_size)
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

    if (pid != 1)
    {
        chsend(to_parent_fd, &send_signal, 1);
        if (send_signal == MIMPI_BCAST_GOOD)
        {
            generalized_send(to_parent_fd, data, count);
        }

        generalized_recieve(from_parent_fd, data, count);
        if (MIMPI_BCAST_neighbour_left(signal, from_parent_fd))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }

    if (2 * pid <= world_size)
    {
        signal = MIMPI_BARIER;
        generalized_send(to_leftson, data, count);
    }
    if (2 * pid + 1 <= world_size)
    {
        signal = MIMPI_BARIER;
        generalized_send(to_rightson, data, count);
    }
    return MIMPI_SUCCESS;
}

uint8_t operation(uint8_t first, uint8_t second, MIMPI_Op op){
    switch (op)
    {
    case MIMPI_MAX:
        if (first > second)
        {
            return first;
        } else {
            return second;
        }
        break;
    case MIMPI_MIN:
        if (first < second)
        {
            return first;
        } else {
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
}

MIMPI_Retcode MIMPI_Reduce(
    void const *send_data,
    void *recv_data,
    int count,
    MIMPI_Op op,
    int root)
{
    GOOD_RANK_BCAST(root);
    char signal;
    char send_signal = pid == root ? MIMPI_BCAST_GOOD : MIMPI_BCAST_BAD;
    uint8_t* left_recieve = NULL;
    uint8_t* right_recieve = NULL;
    uint8_t* send_buffer = malloc(count * sizeof(uint8_t));
    memcpy(send_buffer, send_data, count);

    if (2 * pid <= world_size)
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
    if (2 * pid + 1 <= world_size)
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
    

    if (pid != 1)
    {
        chsend(to_parent_fd, &send_signal, 1);
        if (send_signal == MIMPI_BCAST_GOOD)
        {
            generalized_send(to_parent_fd, send_buffer, count);
        }

        generalized_recieve(from_parent_fd, pid == root ? recv_data : send_buffer, count);
        if (MIMPI_BCAST_neighbour_left(signal, from_parent_fd))
        {
            return MIMPI_ERROR_REMOTE_FINISHED;
        }
    }

    if (2 * pid <= world_size)
    {
        signal = MIMPI_BARIER;
        generalized_send(to_leftson, send_buffer, count);
    }
    if (2 * pid + 1 <= world_size)
    {
        signal = MIMPI_BARIER;
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