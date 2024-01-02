/**
 * This file is for declarations of  common interfaces used in both
 * MIMPI library (mimpi.c) and mimpirun program (mimpirun.c).
 * */

#ifndef MIMPI_COMMON_H
#define MIMPI_COMMON_H

#include <assert.h>
#include <stdbool.h>
#include <stdnoreturn.h>

/*
    Assert that expression doesn't evaluate to -1 (as almost every system function does in case of error).

    Use as a function, with a semicolon, like: ASSERT_SYS_OK(close(fd));
    (This is implemented with a 'do { ... } while(0)' block so that it can be used between if () and else.)
*/
#define ASSERT_SYS_OK(expr)                                                                \
    do {                                                                                   \
        if ((expr) == -1)                                                                  \
            syserr(                                                                        \
                "system command failed: %s\n\tIn function %s() in %s line %d.\n\tErrno: ", \
                #expr, __func__, __FILE__, __LINE__                                        \
            );                                                                             \
    } while(0)

/* Assert that expression evaluates to zero (otherwise use result as error number, as in pthreads). */
#define ASSERT_ZERO(expr)                                                                  \
    do {                                                                                   \
        int const _errno = (expr);                                                         \
        if (_errno != 0)                                                                   \
            syserr(                                                                        \
                "Failed: %s\n\tIn function %s() in %s line %d.\n\tErrno: ",                \
                #expr, __func__, __FILE__, __LINE__                                        \
            );                                                                             \
    } while(0)

/* Prints with information about system error (errno) and quits. */
_Noreturn extern void syserr(const char* fmt, ...);

/* Prints (like printf) and quits. */
_Noreturn extern void fatal(const char* fmt, ...);

#define TODO fatal("UNIMPLEMENTED function %s", __PRETTY_FUNCTION__);


/////////////////////////////////////////////
// Put your declarations here

#define WORLD_SIZE 2

#define MIMPI_INIT 1

#define MIMPI_FINALIZE 0

#define MIMPI_SEND 3

#define MIMPI_RECIEVE 4

#define MIMPI_BARIER 5

#define MIMPI_LEFT 6

#define MIMPI_BCAST_GOOD 7

#define MIMPI_BCAST_BAD 8

#define ERROR 1

struct buffer {
    void* buffor;
    int count;
    int tag;
    int source;
    struct buffor* next;
};
typedef struct buffer buffer_t;// TODO: skopiuj kod ze snake'a 

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
        free(list->buffor);
        prev->next = list->next;
        free(list);
    }
    free(prev);
}

#endif // MIMPI_COMMON_H