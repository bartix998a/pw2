/**
 * This file is for declarations of  common interfaces used in both
 * MIMPI library (mimpi.c) and mimpirun program (mimpirun.c).
 * */

#ifndef MIMPI_COMMON_H
#define MIMPI_COMMON_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/mman.h>
#include <pthread.h>
#include <inttypes.h>
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

#define MIMPI_RECIEVE_DEADLOCK_DETECTION 9

#define ERROR 1

#define DEADLOCK 2

struct buffer {
    void* buffor;
    int count;
    int tag;
    int source;
    struct buffer* next;
};
typedef struct buffer buffer_t;

void push_back(buffer_t *list, buffer_t *element);

buffer_t *find_first(buffer_t *list, int count, int source, int tag);

void remove_element(buffer_t *list, buffer_t *element);


void remove_all(buffer_t *list);

#endif // MIMPI_COMMON_H