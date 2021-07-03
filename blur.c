/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2009 Chris Wilson
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <math.h>
#include "blur.h"
/* Performs a simple 2D Gaussian blur of standard devation @sigma surface @surface. */
void
blur_image_surface (cairo_surface_t *surface, int sigma)
{
    cairo_surface_t *tmp;
    int width, height;
    uint32_t *src, *dst;

    if (cairo_surface_status (surface))
    return;

    width = cairo_image_surface_get_width (surface);
    height = cairo_image_surface_get_height (surface);

    switch (cairo_image_surface_get_format (surface)) {
    case CAIRO_FORMAT_A1:
    default:
    /* Don't even think about it! */
    return;

    case CAIRO_FORMAT_A8:
    /* Handle a8 surfaces by effectively unrolling the loops by a
     * factor of 4 - this is safe since we know that stride has to be a
     * multiple of uint32_t. */
    width /= 4;
    break;

    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
    break;
    }

    tmp = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status (tmp))
    return;

    src = (uint32_t*)cairo_image_surface_get_data (surface);
    dst = (uint32_t*)cairo_image_surface_get_data (tmp);

    // according to a paper by Peter Kovesi [1], box filter of width w, equals to Gaussian blur of following sigma:
    // σ_av = sqrt((w*w-1)/12)
    // for our 7x7 filter we have σ_av = 2.0.
    // applying the same Gaussian filter n times results in σ_n = sqrt(n*σ_av*σ_av) [2]
    // after some trivial math, we arrive at n = ((σ_d)/(σ_av))^2
    // since it's a box blur filter, n >= 3
    //
    // [1]: http://www.peterkovesi.com/papers/FastGaussianSmoothing.pdf
    // [2]: https://en.wikipedia.org/wiki/Gaussian_blur#Mathematics

    int n = lrintf((sigma*sigma)/(SIGMA_AV*SIGMA_AV));
    if (n < 3) n = 3;

    for (int i = 0; i < n; i++)
    {
        // horizontal pass includes image transposition:
        // instead of writing pixel src[x] to dst[x],
        // we write it to transposed location.
        // (to be exact: dst[height * current_column + current_row])
#ifdef __SSE2__
        blur_impl_horizontal_pass_sse2(src, dst, width, height);
        blur_impl_horizontal_pass_sse2(dst, src, height, width);
#else
        blur_impl_horizontal_pass_generic(src, dst, width, height);
        blur_impl_horizontal_pass_generic(dst, src, height, width);
#endif
    }

    cairo_surface_destroy (tmp);
    cairo_surface_flush (surface);
    cairo_surface_mark_dirty (surface);
}

void blur_impl_horizontal_pass_generic(uint32_t *src, uint32_t *dst, int width, int height) {
		uint32_t *o_src = src;
    for (int row = 0; row < height; row++) {
        for (int column = 0; column < width; column++, src++) {
            uint32_t rgbaIn[KERNEL_SIZE + 1];

            // handle borders
            int leftBorder = column < HALF_KERNEL;
            int rightBorder = column > width - HALF_KERNEL;
            int i = 0;
            if (leftBorder) {
                // for kernel size 7x7 and column == 0, we have:
                // x x x P0 P1 P2 P3
                // first loop mirrors P{0..3} to fill x's,
                // second one loads P{0..3}
                for (; i < HALF_KERNEL - column; i++)
                    rgbaIn[i] = *(src + (HALF_KERNEL - i));
                for (; i < KERNEL_SIZE; i++)
                    rgbaIn[i] = *(src - (HALF_KERNEL - i));
            } else if (rightBorder) {
                for (; i < width - column; i++)
                    rgbaIn[i] = *(src + i);
                for (int k = 0; i < KERNEL_SIZE; i++, k++)
                    rgbaIn[i] = *(src - k);
            } else {
                for (; i < KERNEL_SIZE; i++) {
                    if ((uintptr_t) ((src + 4*i - HALF_KERNEL) + 1)
                            > (uintptr_t) (o_src + (height * width)))
                        break;
              			rgbaIn[i] = *(src + i - HALF_KERNEL);
								}
            }

            uint32_t acc[4] = {0};

            for (i = 0; i < KERNEL_SIZE; i++) {
                acc[0] += (rgbaIn[i] & 0xFF000000) >> 24;
                acc[1] += (rgbaIn[i] & 0x00FF0000) >> 16;
                acc[2] += (rgbaIn[i] & 0x0000FF00) >> 8;
                acc[3] += (rgbaIn[i] & 0x000000FF) >> 0;
            }

            for(i = 0; i < 4; i++)
                acc[i] *= 1.0/KERNEL_SIZE;

            *(dst + height * column + row) = (acc[0] << 24) |
                                             (acc[1] << 16) |
                                             (acc[2] << 8 ) |
                                             (acc[3] << 0);
        }
    }
}
