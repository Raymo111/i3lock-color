#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <cairo.h>
#include <jpeglib.h>

#include "jpg.h"

/*
 * Checks if the file is a JPEG by looking for a valid JPEG header.
 */
bool file_is_jpg(FILE* image_file) {
    uint16_t file_header;
    size_t read_count;
    // TODO: Consider endianess on non-x86 platforms
    uint16_t jpg_magick = 0xd8ff;

    fseek(image_file, 0, SEEK_SET);
    read_count = fread(&file_header, sizeof(file_header), 1, image_file);

    if (read_count < 1) {
        fprintf(stderr, "Error searching for JPEG header\n");
        return false;
    }

    return file_header == jpg_magick;
}

/*
 * Reads a JPEG from a file into memory, in a format that Cairo can create a
 * surface from.
 */
void* read_JPEG_file(char *file_path, JPEG_INFO *jpg_info) {
    int img_err;
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *infile;                 /* source file */
    void *img;                    /* decompressed image data pointer */

    if ((infile = fopen(file_path, "rb")) == NULL) {
        img_err = errno;
        fprintf(stderr, "Could not open image file %s: %s\n",
                file_path, strerror(img_err));
        return NULL;
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, infile);

    (void) jpeg_read_header(&cinfo, TRUE);

    // BGRA + endianness change = RGBA?
    // TODO: Test this code on non-x86_64 platforms
    cinfo.out_color_space = JCS_EXT_BGRA;

    (void) jpeg_start_decompress(&cinfo);

    jpg_info->height = cinfo.output_height;
    jpg_info->width = cinfo.output_width;

    /* Get the *cairo* stride rather than the stride from the image. This is
     * the space needed in memory for each row for optimized Cairo rendering. */
    int cairo_stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
            cinfo.output_width);
    jpg_info->stride = cairo_stride;
    if (cairo_stride < jpg_info->width) {
        /* This should never happen, but if it does then the following code
         * will potentially write into unallocated memory */
        fprintf(
            stderr,
            "WARNING: Cairo stride shorter than JPEG width. Aborting JPEG read."
        );
        return NULL;
    }

    // Allocate storage for the final, decompressed image.
    img = calloc(cairo_stride, cinfo.output_height);
    if (img == NULL) {
        fprintf(stderr, "Could not allocate memory for JPEG decode\n");

        (void) jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);

        return NULL;
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        /* Normally, you would allocate a buffer using libJPEG's memory
         * management and write into it, but since we're reading one row at a
         * time, we just write it directly into the image memory space */
        unsigned char* pos = img + (cairo_stride * (cinfo.output_scanline));
        (void) jpeg_read_scanlines(&cinfo, &pos, 1);
    }

    (void) jpeg_finish_decompress(&cinfo);

    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return img;
}
