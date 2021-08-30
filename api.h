#ifndef _API_H
#define _API_H

#include <stdio.h>

typedef struct {
    size_t argc;
    char** argv;
} arg_data;

typedef struct string_t {
    char *value;
    size_t size;
} str_t;

typedef struct arr_str_t {
    str_t *items;
    size_t size;
} arr_t;

void *listen_api(void*);

#endif
