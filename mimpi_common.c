/**
 * This file is for implementation of common interfaces used in both
 * MIMPI library (mimpi.c) and mimpirun program (mimpirun.c).
 * */

#include "mimpi_common.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

_Noreturn void syserr(const char* fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    fprintf(stderr, " (%d; %s)\n", errno, strerror(errno));
    exit(1);
}

_Noreturn void fatal(const char* fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, "\n");
    exit(1);
}

/////////////////////////////////////////////////
// Put your implementation here

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
        if ((tag == 0 ? true : (list->tag == tag)) && list->source == source && list->count == count)
        {
            prev->next = list->next;
            return list;
        }
    }
    return NULL;
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
        
        if (list->buffor != NULL)
        {
            free(list->buffor);
        }
        
        prev->next = list->next;
        free(list);
    }
    free(prev);
}