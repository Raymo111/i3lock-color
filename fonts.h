#ifndef FONTS_H
#define FONTS_H

#include <cairo.h>
#include <cairo-ft.h>
#include <stdbool.h>

#include <rgba.h>

#define VERIF_FONT  0
#define WRONG_FONT  1
#define LAYOUT_FONT 2
#define TIME_FONT   3
#define DATE_FONT   4
#define GREETER_FONT   5

typedef struct text {
    bool show;

    char str[512];
    double size;
    double outline_width;

    cairo_font_face_t *font;

    rgba_t color;
    rgba_t outline_color;
    double x, y;

    int align;
} text_t;

#endif
