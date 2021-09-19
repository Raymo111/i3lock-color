/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2011 Bernhard Eder
 * © 2021 Jezer Mejía
 *
 * See LICENSE for licensing information
 *
 */
#include <cairo/cairo.h>
#include <pthread.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include "cursors.h"
#include "i3lock.h"
#include "api.h"
#include "xcb.h"
#include "unlock_indicator.h"

void update_arguments(int argc, char *argv[], struct option longopts[], char optstring[], bool api);

extern bool debug_mode;
extern char* api_fifo_path;
extern bool api_force_redraw;
extern pthread_mutex_t redraw_mutex;

extern char color[9];

/* options for unlock indicator colors */
extern char insidevercolor[9];
extern char insidewrongcolor[9];
extern char insidecolor[9];
extern char ringvercolor[9];
extern char ringwrongcolor[9];
extern char ringcolor[9];
extern char linecolor[9];
extern char verifcolor[9];
extern char wrongcolor[9];
extern char layoutcolor[9];
extern char timecolor[9];
extern char datecolor[9];
extern char modifcolor[9];
extern char keyhlcolor[9];
extern char bshlcolor[9];
extern char separatorcolor[9];
extern char greetercolor[9];

extern char verifoutlinecolor[9];
extern char wrongoutlinecolor[9];
extern char layoutoutlinecolor[9];
extern char timeoutlinecolor[9];
extern char dateoutlinecolor[9];
extern char greeteroutlinecolor[9];
extern char modifoutlinecolor[9];

extern bool unlock_indicator;
extern background_type_t bg_type;
extern int internal_line_source;

extern bool show_clock;
extern bool always_show_clock;
extern bool show_indicator;
extern bool show_modkey_text;

extern bool blur;
extern bool step_blur;
extern int blur_sigma;

extern double time_size;
extern double date_size;
extern double verif_size;
extern double wrong_size;
extern double modifier_size;
extern double layout_size;
extern double circle_radius;
extern double ring_width;
extern double greeter_size;

extern double timeoutlinewidth;
extern double dateoutlinewidth;
extern double verifoutlinewidth;
extern double wrongoutlinewidth;
extern double modifieroutlinewidth;
extern double layoutoutlinewidth;
extern double greeteroutlinewidth;

extern int verif_align;
extern int wrong_align;
extern int time_align;
extern int date_align;
extern int layout_align;
extern int modif_align;
extern int greeter_align;

extern char ind_x_expr[32];
extern char ind_y_expr[32];
extern char time_x_expr[32];
extern char time_y_expr[32];
extern char date_x_expr[32];
extern char date_y_expr[32];
extern char layout_x_expr[32];
extern char layout_y_expr[32];
extern char status_x_expr[32];
extern char status_y_expr[32];
extern char modif_x_expr[32];
extern char modif_y_expr[32];
extern char verif_x_expr[32];
extern char verif_y_expr[32];
extern char wrong_x_expr[32];
extern char wrong_y_expr[32];
extern char greeter_x_expr[32];
extern char greeter_y_expr[32];

extern char time_format[64];
extern char date_format[64];

extern char verif_font[64];
extern char wrong_font[64];
extern char layout_font[64];
extern char time_font[64];
extern char date_font[64];
extern char greeter_font[64];

extern char* fonts[6];

extern char* verif_text;
extern char* wrong_text;
extern char* noinput_text;
extern char* lock_text;
extern char* lock_failed_text;
extern int   keylayout_mode;
extern char* layout_text;
extern char* greeter_text;

// experimental bar stuff
#define BAR_VERT 0
#define BAR_FLAT 1
#define BAR_DEFAULT 0
#define BAR_REVERSED 1
#define BAR_BIDIRECTIONAL 2
#define MAX_BAR_COUNT 65535
#define MIN_BAR_COUNT 1

extern bool bar_enabled;
extern double *bar_heights;
extern double bar_step;
extern double bar_base_height;
extern double bar_periodic_step;
extern double max_bar_height;
extern int bar_count;
extern int bar_orientation;

extern char bar_base_color[9];
extern char bar_x_expr[32];
extern char bar_y_expr[32]; // empty string on y means use x as offset based on orientation
extern char bar_width_expr[32]; // empty string means full width based on bar orientation
extern bool bar_bidirectional;
extern bool bar_reversed;

extern cairo_surface_t *img;
extern xcb_window_t win;

extern char *image_path;
extern char *image_raw_format;

int is_directory(const char *path);

struct option *api_options;

static void update_api(int argc, char *argv[]) {
    char *optstring = "c:ui:tCFLMrs:kB:m";

    image_path = NULL;
    update_arguments(argc, argv, api_options, optstring, true);

    if (image_path != NULL) {
        if (!is_directory(image_path)) {
            pthread_mutex_lock(&redraw_mutex);
            img = load_image(image_path, image_raw_format);
            pthread_mutex_unlock(&redraw_mutex);
        }
        free(image_path);
    }
    free(image_raw_format);
}

static char** arr_copy(char *arr[], size_t len) {
    size_t str_size = sizeof(*arr) * len;
    char **str = malloc(sizeof(*str) * (len + 1));
    memset(str, 0, str_size);
    for (int i = 0; i < len; i++) {
        str[i] = strdup(arr[i]);
    }
    return str;
}
static void str_push_c(char *str[], char c, size_t *size) {
    int len = strlen(*str);
    if (len + 2 >= *size) {
        *size *= 2;
        *str = realloc(*str, *size * sizeof(char*));
    }
    (*str)[len] = c;
    (*str)[len+1] = '\0';
}
static void arr_push(char **arr[], char *str, size_t *size, size_t *len) {
    if (*len + 1 >= *size) {
        *size *= 2;
        *arr = realloc(*arr, *size * sizeof(char**));
    }
    (*arr)[*len] = strdup(str);
    (*len)++;
}

/**
 * Arguments parser based on "stringtoargcargv.cpp" by Bernhard Eder
 * @see https://web.archive.org/web/20121030075237/http://bbgen.net/blog/2011/06/string-to-argc-argv
 */
static int parse_args(arg_data *data, char str[]) {
    char c;
    int i = 0;
    size_t curr_arg_size = 20;
    char *curr_arg = malloc(curr_arg_size);
    memset(curr_arg, 0, curr_arg_size);

    size_t out_args_size = 10;
    size_t out_args_length = 0;
    char **out_args = malloc(sizeof(char*) * out_args_size);

    enum State {
        InArg,
        InArgQuote,
        OutOfArg,
    };
    enum State curr_state = OutOfArg;
    char curr_quote = '\0';

    while ((c = str[i]) != '\0') {
        if (c == '\"' || c == '\'') { // Quote
            switch (curr_state) {
                case OutOfArg:
                    memset(curr_arg, 0, curr_arg_size);
                case InArg:
                    curr_state = InArgQuote;
                    curr_quote = c;
                    break;
                case InArgQuote:
                    if (c == curr_quote)
                        curr_state = InArg;
                    else
                        str_push_c(&curr_arg, c, &curr_arg_size);
                    break;
            }
        } else
        if (c == ' ' || c == '\t') { // Whitespace
            switch(curr_state) {
                case InArg:
                    arr_push(&out_args, curr_arg, &out_args_size, &out_args_length);
                    curr_state = OutOfArg;
                    break;
                case InArgQuote:
                    str_push_c(&curr_arg, c, &curr_arg_size);
                    break;
                case OutOfArg:
                    break;
            }
        } else {
            switch(curr_state) {
                case InArg:
                case InArgQuote:
                    str_push_c(&curr_arg, c, &curr_arg_size);
                    break;
                case OutOfArg:
                    memset(curr_arg, 0, curr_arg_size);
                    str_push_c(&curr_arg, c, &curr_arg_size);
                    curr_state = InArg;
                    break;
            }
        };
        i++;
    }

    if (curr_state == InArg) {
        arr_push(&out_args, curr_arg, &out_args_size, &out_args_length);
    } else if (curr_state == InArgQuote) {
        return 1;
    }

    size_t argc = out_args_length;
    char **argv = arr_copy(out_args, argc);

    free(out_args);
    free(curr_arg);

    data->argc = argc;
    data->argv = argv;

    return 0;
}

static void init_api_options(struct option longopts[]) {
    int enabled_options[] = {
        'c', 'u', 'i', 998, 't', 'C', 'F', 'L', 'M', 'B',
        // Indicators colors
        300, 301, 302, 303, 304, 305, 306, 307, 308, 309,
        310, 311, 322, 312, 313, 314, 315,
        // Text outline colors
        316, 317, 318, 319, 320, 321, 323, 'r', 's',
        'k', 400, 401, 402, 403,
        // Alignment
        500, 501, 502, 503, 504, 505, 506,
        // String stuff
        510, 511, 512, 513, 514, 515, 516, 517, 518, 519,
        // Fonts
        520, 521, 522, 523, 524, 525,
        // Text size
        530, 531, 532, 533, 534, 535, 536,
        // Text/indicator position
        540, 541, 542, 543, 544, 545, 546, 547, 548,
        // Text outline width
        560, 561, 562, 563, 564, 565, 566,
        // Bar indicator stuff
        700, 701, 703, 704, 705, 706, 707, 708, 709,
        710, 711,

        0
    };
    size_t size = sizeof(enabled_options)/sizeof(*enabled_options);
    api_options = malloc(size * sizeof(struct option));

    int n = 0;
    int i = 0;

    while (n < size) {
        if (longopts[i].val == enabled_options[n]) {
            api_options[n] = longopts[i];
            n++;
        }
        i++;
    }
}

void *listen_api(void* lopts) {
    struct option *longopts = lopts;

    if (access(api_fifo_path, F_OK) == 0) {
        if (remove(api_fifo_path) != 0) {
            fprintf(stderr, "Could not remove file \"%s\": %s\n",
                    api_fifo_path, strerror(errno));
            return NULL;
        }
    }
    if (mkfifo(api_fifo_path, S_IRWXU) != 0) {
        fprintf(stderr, "Could not create FIFO file \"%s\": %s\n",
                api_fifo_path, strerror(errno));
        return NULL;
    }

    FILE *fifo;
    char c;
    int i;
    int times = 0;
    char *text = malloc(50);
    arg_data data;

    DEBUG("Created fifo at \"%s\"\n", api_fifo_path);

    init_api_options(longopts);

#define open_fifo() {\
    fifo = fopen(api_fifo_path, "rb");\
    if (fifo == NULL) {\
        fprintf(stderr, "Could not open FIFO file \"%s\": %s\n", \
                api_fifo_path, strerror(errno));\
        return NULL;\
    }\
}
#define close_fifo() {\
    if (fifo != NULL) fclose(fifo);\
}
    open_fifo()
    bool reading = true;

    while (1) {
        strcpy(text, "command ");
        i = strlen(text);
        int n = i;

        reading = true;
        while (reading && (c = fgetc(fifo)) != EOF) {
            if (sizeof(text) <= i) {
                text = realloc(text, i * 2);
            }
            if (c == '\n') {
                text[i] = ' ';
                reading = false;
            } else
                text[i] = c;
            i++;
        }
        text[i] = '\0';
        if (c == EOF) {
            close_fifo();
            open_fifo();
        }
        if (n == i) continue;

        DEBUG("API message received - %d\n", times);
        DEBUG("Message: \"%s\"\n", text);
        times++;
        if (parse_args(&data, text) != 0) {
            fprintf(stderr, "Could not parse message\n");
            continue;
        }

        update_api(data.argc, data.argv);
        init_colors_once();

        memset(text, 0, i);
        if (api_force_redraw) {
            redraw_screen();
        }
    }
    fclose(fifo);

    return NULL;
}
