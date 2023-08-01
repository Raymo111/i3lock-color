#ifndef _BLUR_H
#define _BLUR_H

#include <stdint.h>
#include <cairo.h>

#define KERNEL_SIZE 7
#define SIGMA_AV 2
#define HALF_KERNEL KERNEL_SIZE / 2

void blur_image_surface(cairo_surface_t *surface, int sigma);
#ifdef __SSE2__
void blur_impl_horizontal_pass_sse2(uint32_t *src, uint32_t *dst, int width, int height);
#endif
void blur_impl_horizontal_pass_generic(uint32_t *src, uint32_t *dst, int width, int height);
#endif
