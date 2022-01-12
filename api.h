#ifndef _API_H
#define _API_H

#include <stdio.h>
#include <pthread.h>

typedef struct {
    size_t argc;
    char** argv;
} arg_data;

void *listen_api(void*);
void close_api(pthread_t api_thread);

#endif
