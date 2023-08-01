#ifndef RGBA_H
#define RGBA_H

typedef struct rgb {
    double red;
    double green;
    double blue;
} rgb_t;

typedef struct rgb_str {
    char red[3];
    char green[3];
    char blue[3];
} rgb_str_t;

typedef struct rgba {
    double red;
    double green;
    double blue;
    double alpha;
} rgba_t;

typedef struct rgba_str {
    char red[3];
    char green[3];
    char blue[3];
    char alpha[3];
} rgba_str_t;

#endif
