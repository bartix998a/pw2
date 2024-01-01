/**
 * This file is for implementation of MIMPI library.
 * */

#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/mman.h>
#include <pthread.h>
#include "channel.h"
#include "mimpi.h"
#include "mimpi_common.h"

#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif


static int pid;
static int from_parent_fd;
static int to_OS_public_fd;
static int to_OS_private_fd;
static int from_OS_fd;
static int to_rightson;
static int to_leftson;
static int bufferChannel;

buffer_t* push_back(buffer_t* list, buffer_t* element);

buffer_t* find_first(buffer_t* list, int count, int source, int tag);

static buffer_t* recieve_buffer;
static int current_tag;
static bool end = false;

void mimpi_send_request(int request){
    int message[2] = {pid, request};
    chsend(to_OS_public_fd, message, 2 * sizeof(int));
}

void init_static_variables() {
    int result = 0;
    sscanf(getenv("MIMPI_ID"), "%d", &result);
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
    sscanf(getenv("MIMPI_to_left_sin"), "%d", &result);
    to_leftson = result;
    sscanf(getenv("MIMPI_from_buffer"), "%d", &result);
    bufferChannel = result;
    recieve_buffer = malloc(sizeof(buffer_t));
    recieve_buffer->tag = -1;
}

void MIMPI_Init(bool enable_deadlock_detection) {
    channels_init();
    init_static_variables();
}

void MIMPI_Finalize() {
    TODO
    // send info to main thread and finish of reciever thread
    mimpi_send_request(MIMPI_FINALIZE);
    channels_finalize();
}

int MIMPI_World_size() {
    int wSize = 0;
    int fd = 0;
    char* str;
    mimpi_send_request(WORLD_SIZE);
    str = getenv("MIMPI_from_OS");
    sscanf(str, "%d", &fd);
    chrecv(fd, &wSize, sizeof(int));
    return wSize;
}

int MIMPI_World_rank() {
    return pid;
}

void recieve(){
    while(!end) {
        int request[3];
        chrecv(bufferChannel, request, 3 * sizeof(int));// main przesyla
        buffer_t* element = malloc(sizeof(buffer_t));
        element->tag = request[2];
        element->count = request[0];
        element->buffor = malloc(request[0]);
        element ->next = NULL;
        for (int i = 0; i < element->count/PIPE_BUF + element->count % PIPE_BUF == 0 ? 0 : 1; i++) {
            chrecv(bufferChannel, recieve_buffer + PIPE_BUF * i, element->count/PIPE_BUF == i ? element->count % 512 : PIPE_BUF);// samo sie zbuforuje (chyba)
        }
        // mutex lock
        push_back(&recieve_buffer, element);
        if (element->tag == current_tag /*czy jakos tak jeszcze czekajacy i */)
        {
            // obudz glowny watek
        }
        
        // mutex unlock and wake up other thread potenetially
    }
}

MIMPI_Retcode MIMPI_Send(
    void const *data,
    int count,
    int destination,
    int tag
) {
    int request[5] = {pid, MIMPI_SEND, count, destination, tag};
    int dest_fd;
    chsend(to_OS_public_fd, request, 5 * sizeof(int));
    chrecv(from_OS_fd, &dest_fd, sizeof(int));
    if (dest_fd != 0)
    {
        for (int i = 0; i < count/PIPE_BUF + count % PIPE_BUF == 0 ? 0 : 1; i++) {
            chsend(dest_fd, data + PIPE_BUF * i, count/PIPE_BUF == i ? count%512 : PIPE_BUF);// samo sie zbuforuje (chyba)
        }
    } else {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    return MIMPI_SUCCESS;
    TODO
}

MIMPI_Retcode MIMPI_Recv(
    void *data,
    int count,
    int source,
    int tag
) {
    int response;
    mimpi_send_request(MIMPI_RECIEVE);
    chrecv(from_OS_fd, &response, sizeof(int));
    if (response == ERROR)
    {
        return MIMPI_ERROR_REMOTE_FINISHED;
    }
    
    // lock mutex
    buffer_t* element = find_first(recieve_buffer, count, source, tag);
    if (element == NULL){
        current_tag = tag; 
        // unlock mutex
        // wait on mutex2
    }
    memcpy(data, element->buffor, element->count);
    free(element->buffor);
    free(element);
    return MIMPI_SUCCESS;
}

MIMPI_Retcode MIMPI_Barrier() {
    TODO
}

MIMPI_Retcode MIMPI_Bcast(
    void *data,
    int count,
    int root
) {
    TODO
}

MIMPI_Retcode MIMPI_Reduce(
    void const *send_data,
    void *recv_data,
    int count,
    MIMPI_Op op,
    int root
) {
    TODO
}