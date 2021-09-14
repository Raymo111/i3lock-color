#ifndef _API_H
#define _API_H

#include <stdio.h>

typedef struct {
    size_t argc;
    char** argv;
} arg_data;

void *listen_api(void*);

#endif
