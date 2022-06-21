#ifndef _JPG_H
#define _JPG_H

#include <sys/types.h>

#define _GNU_SOURCE 1
typedef struct {
    uint height;
    uint width;
    uint stride; // The width of each row in memory, in bytes
} JPEG_INFO;

/*
 * Checks if the file is a JPEG by looking for a valid JPEG header.
 */
bool file_is_jpg(FILE* image_file);

/*
 * Reads a JPEG from a file into memory, in a format that Cairo can create a
 * surface from.
 */
void* read_JPEG_file(char *filename, JPEG_INFO *jpg_info);

#endif
