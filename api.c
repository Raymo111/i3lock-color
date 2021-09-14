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

static void update_api(int argc, char *argv[]) {
    struct option longopts[] = {
        {"color", required_argument, NULL, 'c'},
        {"no-unlock-indicator", no_argument, NULL, 'u'},
        {"image", required_argument, NULL, 'i'},
        {"raw", required_argument, NULL, 998},
        {"tiling", no_argument, NULL, 't'},
        {"centered", no_argument, NULL, 'C'},
        {"fill", no_argument, NULL, 'F'},
        {"scale", no_argument, NULL, 'L'},
        {"max", no_argument, NULL, 'M'},
        {"blur", required_argument, NULL, 'B'},

        // options for unlock indicator colors
        {"insidever-color", required_argument, NULL, 300},
        {"insidewrong-color", required_argument, NULL, 301},
        {"inside-color", required_argument, NULL, 302},
        {"ringver-color", required_argument, NULL, 303},
        {"ringwrong-color", required_argument, NULL, 304},
        {"ring-color", required_argument, NULL, 305},
        {"line-color", required_argument, NULL, 306},
        {"verif-color", required_argument, NULL, 307},
        {"wrong-color", required_argument, NULL, 308},
        {"layout-color", required_argument, NULL, 309},
        {"time-color", required_argument, NULL, 310},
        {"date-color", required_argument, NULL, 311},
        {"modif-color", required_argument, NULL, 322},
        {"keyhl-color", required_argument, NULL, 312},
        {"bshl-color", required_argument, NULL, 313},
        {"separator-color", required_argument, NULL, 314},
        {"greeter-color", required_argument, NULL, 315},

        // text outline colors
        {"verifoutline-color", required_argument, NULL, 316},
        {"wrongoutline-color", required_argument, NULL, 317},
        {"layoutoutline-color", required_argument, NULL, 318},
        {"timeoutline-color", required_argument, NULL, 319},
        {"dateoutline-color", required_argument, NULL, 320},
        {"greeteroutline-color", required_argument, NULL, 321},
        {"modifoutline-color", required_argument, NULL, 323},

        {"line-uses-ring", no_argument, NULL, 'r'},
        {"line-uses-inside", no_argument, NULL, 's'},

        {"clock", no_argument, NULL, 'k'},
        {"force-clock", no_argument, NULL, 400},
        {"indicator", no_argument, NULL, 401},
        {"radius", required_argument, NULL, 402},
        {"ring-width", required_argument, NULL, 403},

        // alignment
        {"time-align", required_argument, NULL, 500},
        {"date-align", required_argument, NULL, 501},
        {"verif-align", required_argument, NULL, 502},
        {"wrong-align", required_argument, NULL, 503},
        {"layout-align", required_argument, NULL, 504},
        {"modif-align", required_argument, NULL, 505},
        {"greeter-align", required_argument, NULL, 506},

        // string stuff
        {"time-str", required_argument, NULL, 510},
        {"date-str", required_argument, NULL, 511},
        {"verif-text", required_argument, NULL, 512},
        {"wrong-text", required_argument, NULL, 513},
        {"keylayout", required_argument, NULL, 514},
        {"noinput-text", required_argument, NULL, 515},
        {"lock-text", required_argument, NULL, 516},
        {"lockfailed-text", required_argument, NULL, 517},
        {"greeter-text", required_argument, NULL, 518},
        {"no-modkey-text", no_argument, NULL, 519},

        // fonts
        {"time-font", required_argument, NULL, 520},
        {"date-font", required_argument, NULL, 521},
        {"verif-font", required_argument, NULL, 522},
        {"wrong-font", required_argument, NULL, 523},
        {"layout-font", required_argument, NULL, 524},
        {"greeter-font", required_argument, NULL, 525},

        // text size
        {"time-size", required_argument, NULL, 530},
        {"date-size", required_argument, NULL, 531},
        {"verif-size", required_argument, NULL, 532},
        {"wrong-size", required_argument, NULL, 533},
        {"layout-size", required_argument, NULL, 534},
        {"modif-size", required_argument, NULL, 535},
        {"greeter-size", required_argument, NULL, 536},

        // text/indicator positioning
        {"time-pos", required_argument, NULL, 540},
        {"date-pos", required_argument, NULL, 541},
        {"verif-pos", required_argument, NULL, 542},
        {"wrong-pos", required_argument, NULL, 543},
        {"layout-pos", required_argument, NULL, 544},
        {"status-pos", required_argument, NULL, 545},
        {"modif-pos", required_argument, NULL, 546},
        {"ind-pos", required_argument, NULL, 547},
        {"greeter-pos", required_argument, NULL, 548},

        // text outline width
        {"timeoutline-width", required_argument, NULL, 560},
        {"dateoutline-width", required_argument, NULL, 561},
        {"verifoutline-width", required_argument, NULL, 562},
        {"wrongoutline-width", required_argument, NULL, 563},
        {"modifieroutline-width", required_argument, NULL, 564},
        {"layoutoutline-width", required_argument, NULL, 565},
        {"greeteroutline-width", required_argument, NULL, 566},

        // bar indicator stuff
        {"bar-indicator", no_argument, NULL, 700},
        {"bar-direction", required_argument, NULL, 701},
        {"bar-orientation", required_argument, NULL, 703},
        {"bar-step", required_argument, NULL, 704},
        {"bar-max-height", required_argument, NULL, 705},
        {"bar-base-width", required_argument, NULL, 706},
        {"bar-color", required_argument, NULL, 707},
        {"bar-periodic-step", required_argument, NULL, 708},
        {"bar-pos", required_argument, NULL, 709},
        {"bar-count", required_argument, NULL, 710},
        {"bar-total-width", required_argument, NULL, 711},

        {NULL, no_argument, NULL, 0}};

    char *optstring = "c:ui:tCFLMrs:kB:m";

    image_path = NULL;
    update_arguments(argc, argv, longopts, optstring, true);

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

void *listen_api(void* _) {

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
