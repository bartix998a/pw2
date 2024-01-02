/**
 * This file is for implementation of mimpirun program.
 * */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "mimpi_common.h"
#include "channel.h"


#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif


extern char **environ;

void runMIMPIOS(int n, int*** toChlidren, int* toMIOS, int** tree, int** toBuffer, int** request_to_buffer){
    int request[2]; // request[0] - who sent it, request[1] - request proper
    int send_request[3];
    int response[10];
    bool* initialized = (bool*) malloc(n * sizeof(bool));
    for (int i = 0; i < n; i++)
    {
        initialized[i] = 0;
    }
    int leftMIMPI = 0;
    int init_count = 0;
    while (true)
    {
        chrecv(toMIOS[0], request, 2 * sizeof(int));
        switch (request[1])
        {
        case WORLD_SIZE:
            response[0] = n;
            chsend(toChlidren[request[0]][0][1], response, sizeof(int));
            break;
        case MIMPI_INIT:
            initialized[request[0] - 1] = true;
            init_count++;
            break;
        case MIMPI_FINALIZE:
            initialized[request[0] - 1] = false;
            init_count--;
            leftMIMPI++;
            int sigkill[3] = {0, 0, 0};
            chsend(request_to_buffer[request[0]][1], sigkill, 3 * sizeof(int));
            if (leftMIMPI == n)
            {                
                free(initialized);
                return;
            }
            break;
        case MIMPI_SEND:
            chrecv(toMIOS[0], send_request, 3 * sizeof(int));
            if (send_request[2] == 0) {
                printf("blad nie wiem co robic");
                exit(1);
            }
            if (!initialized[send_request[1]])
            {
                response[0] = 0;
                chsend(toChlidren[request[0]][0][1], response, sizeof(int));
            } else {
                response[0] = toBuffer[send_request[1]][1];
                int destination = send_request[1];
                chsend(request_to_buffer[destination][1], send_request, 3 * sizeof(int));
                chsend(toChlidren[request[0]][0][1], response, sizeof(int));
            }
        }        
    }
    
}

void fillWithZero(char* ar){
    int i = 0;
    while (ar[i] != 0)
    {
        ar[i] = 0;
        i++;
    }
    
}

int main(int argc, char** argv) {
    int n = atoi(argv[1]);
    int pid = 0;
    int* toMIOS = (int*) malloc(2 * sizeof(int));// write to the second one
    int*** toChildren = (int***) malloc((n + 1) * sizeof(int**));// [x][0][x] - for os to send
    int** tree = (int**) malloc((n + 1) * sizeof(int*));
    int** reverse_tree = (int**) malloc((n + 1) * sizeof(int*));
    int** toBuffer = (int**) malloc((n + 1) * sizeof(int*));
    int** os_to_buffer = (int**) malloc((n + 1) * sizeof(int*));
    for (int i = 0; i < n + 1; i++)
    {
        toChildren[i] = (int**) malloc(2 * sizeof(int*));
        toChildren[i][0] = (int*) malloc(2 * sizeof(int));
        toChildren[i][1] = (int*) malloc(2 * sizeof(int));
        tree[i] = malloc(2 * sizeof(int));
        reverse_tree[i] = malloc(2 * sizeof(int));
        toBuffer[i] = malloc(2 * sizeof(int));
        toBuffer[i][0] = 0;
        toBuffer[i][1] = 0;
        os_to_buffer[i] = malloc(2 * sizeof(int));
    }
    
    char temp[16];
    temp[9] = 0;

    if (argc < 3)
    {
        return 1;
    }

    channel(toMIOS);
    for (size_t i = 0; i <= n; i++)
    {
        channel(toChildren[i][0]);
        channel(toChildren[i][1]);
        channel(tree[i]);
        channel(reverse_tree[i]);
        channel(toBuffer[i]);
        channel(os_to_buffer[i]);
    }
    for (size_t i = 0; i < n && pid == 0; i++)
    {
        pid = fork();
        if (pid != 0)
        {
            pid = i + 1;
        }
        
    }
    
    if (pid != 0) {
        sprintf(temp, "%d", toMIOS[1]);
        setenv("MIMPI_to_OS_public", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", toChildren[pid][0][0]);
        setenv("MIMPI_from_OS", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", toChildren[pid][1][1]);
        setenv("MIMPI_to_OS_private", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", pid);
        setenv("MIMPI_ID", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", tree[pid / 2][0]);
        setenv("MIMPI_from_parent", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", reverse_tree[pid / 2][1]);
        setenv("MIMPI_to_parent", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", 2 * pid <= n ? tree[2 * pid][1] : -1);
        setenv("MIMPI_to_left_son", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", ((2 * pid) <= n ? reverse_tree[2 * pid][0] : -1));
        setenv("MIMPI_from_left_son", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", 2 * pid + 1 <= n ? tree[2 * pid + 1][1] : - 1);
        setenv("MIMPI_to_right_son", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", 2 * pid + 1 <= n ? reverse_tree[2 * pid + 1][0] : - 1);
        setenv("MIMPI_from_right_son", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", toBuffer[pid][0]);
        setenv("MIMPI_from_buffer", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", n);
        setenv("MIMPI_world_size", argv[1], 1);
        
        fillWithZero(temp);
        sprintf(temp, "%d", os_to_buffer[pid][0]);
        setenv("MIMPI_from_OS_buffer", temp, 1);
        ASSERT_SYS_OK(execvpe(argv[2], &(argv[2]), environ));
    }
    
    // here is only one process with pid == 0
    runMIMPIOS(n, toChildren, toMIOS, tree, toBuffer, os_to_buffer);
    close(toMIOS[0]);
    close(toMIOS[1]);
    for (int i = 0; i <= n; i++)
    {
        close(toChildren[i][0][0]);
        close(toChildren[i][0][1]);
        close(toChildren[i][1][0]);
        close(toChildren[i][1][1]);
        close(tree[i][1]);
        close(tree[i][0]);
        close(os_to_buffer[i][1]);
        close(os_to_buffer[i][0]);
        close(toBuffer[i][1]);
        close(toBuffer[i][0]);
        
    }

    for (int i = 0; i < n + 1; i++)
    {
        free(toChildren[i][0]);
        free(toChildren[i][1]);
        free(toChildren[i]);
        free(tree[i]);
        free(reverse_tree[i]);
        free(toBuffer[i]);
        free(os_to_buffer[i]);
    }
    free(tree);
    free(reverse_tree);
    free(toBuffer);
    free(os_to_buffer);
    free(toChildren);
    free(toMIOS);
    printf("wait\n");

    for (int i = 0; i < n; i++)
    {
        int temp = 0;
        wait(&temp);
    }
    
    printf("exit\n");
    return 0;
}