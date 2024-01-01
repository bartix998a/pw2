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
#include <semaphore.h>
#include "mimpi_common.h"
#include "channel.h"

extern char **environ;

void runMIMPIOS(int n, int*** toChlidren, int* toMIOS, int** tree, int** toBuffer){
    int request[2]; // request[0] - who sent it, request[1] - request proper
    int send_request[3];
    int response[10];
    void* buffor = malloc(512);
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
            initialized--;
            leftMIMPI++;
            if (leftMIMPI == n)
            {
                free(initialized);
                free(buffor);
                return;
            }
            break;
        case MIMPI_SEND:
            chrecv(toMIOS[0], send_request, 3 * sizeof(int));
            if (!initialized[send_request[1]])
            {
                response[0] = 0;
                chsend(toChlidren[request[0]][0][1], response, sizeof(int));
            } else {
                response[0] = toBuffer[send_request[1]][1];
                int destination = send_request[1];
                chsend(toChlidren[destination][0][1], send_request, 3 * sizeof(int));
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
    int tree[n + 1][2];
    int toBuffer[n + 1][2];
    for (int i = 0; i < n + 1; i++)
    {
        toChildren[i] = (int***) malloc(2 * sizeof(int*));
        toChildren[i][0] = (int**) malloc(2 * sizeof(int));
        toChildren[i][1] = (int**) malloc(2 * sizeof(int));
    }
    
    char temp[16];
    temp[9] = 0;

    if (argc != 4)
    {
        return 1;
    }

    channels_init();
    channel(toMIOS);
    channel(toChildren[n][0]);
    channel(toChildren[n][1]);
    channel(tree[n]);

    for (size_t i = 0; pid == 0 && i < n; i++)
    {
        channel(toChildren[i][0]);
        channel(toChildren[i][1]);
        channel(tree[i]);
        channel(toBuffer[i]);
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
        sprintf(temp, "%d", 2 * pid <= n ? tree[2 * pid][1] : -1);
        setenv("MIMPI_to_left_son", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", 2 * pid + 1 <= n ? tree[2 * pid + 1][1] : - 1);
        setenv("MIMPI_to_right_son", temp, 1);

        fillWithZero(temp);
        sprintf(temp, "%d", toBuffer[pid][0]);
        setenv("MIMPI_from_buffer", temp, 1);

        fillWithZero(temp);
        setenv("MIMPI_world_size", argv[1], 1);
        
        ASSERT_SYS_OK(execvpe(argv[2], &(argv[2]), environ));
    }
    
    // here is only one process with pid == 0
    runMIMPIOS(n, toChildren, toMIOS, tree, toBuffer);
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
    }
    for (int i = 0; i < n; i++)
    {
        int temp = 0;
        wait(&temp);
    }
    
    return 0;
}