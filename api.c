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
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include "cursors.h"
#include "i3lock.h"
#include "api.h"
#include "xcb.h"
#include "unlock_indicator.h"

extern bool debug_mode;
extern char* api_fifo_path;
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

int is_directory(const char *path);

static void update_arguments(int argc, char *argv[]) {
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

    int o = 0;
    int longoptind = 0;
    char *optstring = "c:ui:tCFLMrs:kB:m";
    char *arg = NULL;
    int opt = 0;

    char padded[9] = "ffffffff"; \

#define parse_color(acolor)\
    arg = optarg;\
    if (arg[0] == '#') arg++;\
    if (strlen(arg) == 6) {\
      /* If 6 digits given, assume RGB and pad 0xff for alpha */\
      strncpy( padded, arg, 6 );\
      arg = padded;\
    }\
    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", acolor) != 1) {\
        fprintf(stderr, #acolor " is invalid, color must be given in 3 or 4-bye format: rrggbb[aa]\n");\
        return;\
    }

#define parse_outline_width(awidth)\
    arg = optarg;\
    if (sscanf(arg, "%lf", &awidth) != 1) {\
        fprintf(stderr, #awidth " must be a number.\n");\
        return;\
    }\
    if (awidth < 0) {\
        fprintf(stderr, #awidth " must be a positive double; ignoring...\n");\
        awidth = 0;\
    }

#define apierr(fmt, ...) {\
        fprintf(stderr, "[i3lock-error] " fmt, ##__VA_ARGS__);\
        return;\
    }

    char *image_path = NULL;
    char *image_raw_format = NULL;

    optind = 0;

    while ((o = getopt_long(argc, argv, optstring, longopts, &longoptind)) != -1) {
        DEBUG("OPTLONG: %d: - %s\n", o, optarg);
        switch (o) {
            case 'u':
                unlock_indicator = false;
                break;
            case 'i':
                image_path = strdup(optarg);
                break;
            case 't':
                bg_type = TILE;
                break;
            case 'C':
                bg_type = CENTER;
                break;
            case 'F':
                bg_type = FILL;
                break;
            case 'L':
                bg_type = SCALE;
                break;
            case 'M':
                bg_type = MAX;
                break;
            case 'r':
                internal_line_source = 1; //sets the line drawn inside to use the inside color when drawn
                break;
            case 's':
                internal_line_source = 2;
                break;
            case 'k':
                show_clock = true;
                break;
            case 'B':
                blur = true;
                blur_sigma = atoi(optarg);
                break;

            // Begin colors
            case 'c':
                parse_color(color);
                break;
            case 300:
                parse_color(insidevercolor);
                break;
            case 301:
                parse_color(insidewrongcolor);
                break;
            case 302:
                parse_color(insidecolor);
                break;
            case 303:
                parse_color(ringvercolor);
                break;
            case 304:
                parse_color(ringwrongcolor);
                break;
            case 305:
                parse_color(ringcolor);
                break;
            case 306:
                parse_color(linecolor);
                break;
            case 307:
                parse_color(verifcolor);
                break;
            case 308:
                parse_color(wrongcolor);
                break;
            case 309:
                parse_color(layoutcolor);
                break;
            case 310:
                parse_color(timecolor);
                break;
            case 311:
                parse_color(datecolor);
                break;
            case 312:
                parse_color(keyhlcolor);
                break;
            case 313:
                parse_color(bshlcolor);
                break;
            case 314:
                parse_color(separatorcolor);
                break;
            case 315:
                parse_color(greetercolor);
                break;
            case  316:
                parse_color(verifoutlinecolor);
                break;
            case  317:
                parse_color(wrongoutlinecolor);
                break;
            case  318:
                parse_color(layoutoutlinecolor);
                break;
            case  319:
                parse_color(timeoutlinecolor);
                break;
            case  320:
                parse_color(dateoutlinecolor);
                break;
            case  321:
                parse_color(greeteroutlinecolor);
                break;
            case  322:
                parse_color(modifcolor);
                break;
            case  323:
                parse_color(modifoutlinecolor);
                break;

			// General indicator opts
            case 400:
                show_clock = true;
                always_show_clock = true;
                break;
            case 401:
                show_indicator = true;
                break;
            case 402:
                arg = optarg;
                if (sscanf(arg, "%lf", &circle_radius) != 1) {
                    apierr("radius must be a number.\n");
                }
                if (circle_radius < 1) {
                    fprintf(stderr, "radius must be a positive integer; ignoring...\n");
                    circle_radius = 90.0;
                }
                break;
            case 403:
                arg = optarg;
                if (sscanf(arg, "%lf", &ring_width) != 1) {
                    apierr("ring-width must be a number.\n");
                }
                if (ring_width < 1.0) {
                    fprintf(stderr, "ring-width must be a positive float; ignoring...\n");
                    ring_width = 7.0;
                }
                break;

			// Alignment stuff
            case 500:
                opt = atoi(optarg);
                if (opt < 0 || opt > 2) opt = 0;
                time_align = opt;
                break;
            case 501:
                opt = atoi(optarg);
                if (opt < 0 || opt > 2) opt = 0;
                date_align = opt;
                break;
            case 502:
                opt = atoi(optarg);
                if (opt < 0 || opt > 2) opt = 0;
                verif_align = opt;
                break;
            case 503:
                opt = atoi(optarg);
                if (opt < 0 || opt > 2) opt = 0;
                wrong_align = opt;
                break;
            case 504:
                opt = atoi(optarg);
                if (opt < 0 || opt > 2) opt = 0;
                layout_align = opt;
                break;
            case 505:
                opt = atoi(optarg);
                if (opt < 0 || opt > 2) opt = 0;
                modif_align = opt;
                break;
            case 506:
                opt = atoi(optarg);
                if (opt < 0 || opt > 2) opt = 0;
                greeter_align = opt;
                break;

			// String stuff
            case 510:
                if (strlen(optarg) > 31) {
                    apierr("time format string can be at most 31 characters.\n");
                }
                strcpy(time_format,optarg);
                break;
            case 511:
                if (strlen(optarg) > 31) {
                    apierr("radius must be a number.\n");
                }
                strcpy(date_format,optarg);
                break;
            case 512:
                verif_text = optarg;
                break;
            case 513:
                wrong_text = optarg;
                break;
            case 514:
                // if layout is NULL, do nothing
                // if not NULL, attempt to display stuff
                // need to code some sane defaults for it
                keylayout_mode = atoi(optarg);
                break;
            case 515:
                noinput_text = optarg;
                break;
            case 516:
                lock_text = optarg;
                break;
            case 517:
                lock_failed_text = optarg;
                break;
            case 518:
                greeter_text = optarg;
                break;
            case 519:
                show_modkey_text = false;
                break;

			// Font stuff
            case 520:
                if (strlen(optarg) > 63) {
                    apierr("time fotn string can be at most 63 characters.\n");
                }
                strcpy(fonts[TIME_FONT],optarg);
                break;
            case 521:
                if (strlen(optarg) > 63) {
                    apierr("date font string can be at most 63 characters.\n");
                }
                strcpy(fonts[DATE_FONT],optarg);
                break;
            case 522:
                if (strlen(optarg) > 63) {
                    apierr("verif cont string can be at most 63 characters.\n");
                }
                strcpy(fonts[VERIF_FONT],optarg);
                break;
            case 523:
                if (strlen(optarg) > 63) {
                    apierr("wrong font string can be at most 63 characters.\n");
                }
                strcpy(fonts[WRONG_FONT],optarg);
                break;
            case 524:
                if (strlen(optarg) > 63) {
                    apierr("layout font string can be at most 63 characters.\n");
                }
                strcpy(fonts[LAYOUT_FONT],optarg);
                break;
            case 525:
                if (strlen(optarg) > 63) {
                    apierr("greeter font string can be at most 63 characters.\n");
                }
                strcpy(fonts[GREETER_FONT],optarg);
                break;

			// Text size
            case 530:
                arg = optarg;
                if (sscanf(arg, "%lf", &time_size) != 1) {
                    apierr("timesize must be a number.\n");
                }
                if (time_size < 1) {
                    apierr("timesize must be larger than 0.\n");
                }
                break;
            case 531:
                arg = optarg;
                if (sscanf(arg, "%lf", &date_size) != 1) {
                    apierr("datesize must be a number.\n");
                }
                if (date_size < 1) {
                    apierr("datesize must be larger than 0.\n");
                }
                break;
            case 532:
                arg = optarg;
                if (sscanf(arg, "%lf", &verif_size) != 1) {
                    apierr("verifsize must be a number.\n");
                }
                if (verif_size < 1) {
                    fprintf(stderr, "verifsize must be a positive integer; ignoring...\n");
                    verif_size = 28.0;
                }
                break;
            case 533:
                arg = optarg;
                if (sscanf(arg, "%lf", &wrong_size) != 1) {
                    apierr("wrongsize must be a number.\n");
                }
                if (wrong_size < 1) {
                    fprintf(stderr, "wrongsize must be a positive integer; ignoring...\n");
                    wrong_size = 28.0;
                }
                break;
            case 534:
                arg = optarg;
                if (sscanf(arg, "%lf", &layout_size) != 1) {
                    apierr("layoutsize must be a number.\n");
                }
                if (date_size < 1) {
                    apierr("layoutsize must be larger than 0.\n");
                }
                break;
            case 535:
                arg = optarg;
                if (sscanf(arg, "%lf", &modifier_size) != 1) {
                    apierr("modsize must be a number.\n");
                }
                if (modifier_size < 1) {
                    fprintf(stderr, "modsize must be a positive integer; ignoring...\n");
                    modifier_size = 14.0;
                }
                break;
            case 536:
                arg = optarg;
                if (sscanf(arg, "%lf", &greeter_size) != 1) {
                    apierr("greetersize must be a number.\n");
                }
                if (greeter_size < 1) {
                    fprintf(stderr, "greetersize must be a positive integer; ignoring...\n");
                    greeter_size = 14.0;
                }
                break;

			// Positions
            case 540:
                //read in to time_x_expr and time_y_expr
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    apierr("time position string can be at most 31 characters.\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", time_x_expr, time_y_expr) != 2) {
                    apierr("timepos must be of the form x:y.\n");
                }
                break;
            case 541:
                //read in to date_x_expr and date_y_expr
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    apierr("date position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", date_x_expr, date_y_expr) != 2) {
                    apierr("datepos must be of the form x:y\n");
                }
                break;
            case 542:
                // read in to time_x_expr and time_y_expr
                if (strlen(optarg) > 31) {
                    apierr("verif position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", verif_x_expr, verif_y_expr) != 2) {
                    apierr("verifpos must be of the form x:y\n");
                }
                break;
            case 543:
                if (strlen(optarg) > 31) {
                    apierr("\"wrong\" text position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", wrong_x_expr, wrong_y_expr) != 2) {
                    apierr("verifpos must be of the form x:y\n");
                }
                break;
            case 544:
                if (strlen(optarg) > 31) {
                    apierr("layout position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", layout_x_expr, layout_y_expr) != 2) {
                    apierr("layoutpos must be of the form x:y\n");
                }
                break;
            case 545:
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    apierr("status position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", status_x_expr, status_y_expr) != 2) {
                    apierr("statuspos must be of the form x:y\n");
                }
                break;
            case 546:
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    apierr("modif position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", modif_x_expr, modif_y_expr) != 2) {
                    apierr("modifpos must be of the form x:y\n");
                }
                break;
            case 547:
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    apierr("indicator position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", ind_x_expr, ind_y_expr) != 2) {
                    apierr("indpos must be of the form x:y\n");
                }
                break;
            case 548:
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    apierr("indicator position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", greeter_x_expr, greeter_y_expr) != 2) {
                    apierr("indpos must be of the form x:y\n");
                }
                break;

            // text outline width
            case 560:
                parse_outline_width(timeoutlinewidth);
                break;
            case 561:
                parse_outline_width(dateoutlinewidth);
                break;
            case 562:
                parse_outline_width(verifoutlinewidth);
                break;
            case 563:
                parse_outline_width(wrongoutlinewidth);
                break;
            case 564:
                parse_outline_width(modifieroutlinewidth);
                break;
            case 565:
                parse_outline_width(layoutoutlinewidth);
                break;
            case 566:
                parse_outline_width(greeteroutlinewidth);
                break;

			// Bar indicator
            case 700:
                bar_enabled = true;
                break;
            case 701:
                opt = atoi(optarg);
                switch(opt) {
                    case BAR_REVERSED:
                        bar_reversed = true;
                        break;
                    case BAR_BIDIRECTIONAL:
                        bar_bidirectional = true;
                        break;
                    case BAR_DEFAULT:
                    default:
                        break;
                }
                break;
            case 703:
                arg = optarg;
                if (strcmp(arg, "vertical") == 0)
                    bar_orientation = BAR_VERT;
                else if (strcmp(arg, "horizontal") == 0)
                    bar_orientation = BAR_FLAT;
                else
                    apierr("bar orientation must be \"vertical\" or \"horizontal\"\n");
                break;
            case 704:
                bar_step = atoi(optarg);
                if (bar_step < 1) bar_step = 15;
                break;
            case 705:
                max_bar_height = atoi(optarg);
                if (max_bar_height < 1) max_bar_height = 25;
                break;
            case 706:
                bar_base_height = atoi(optarg);
                if (bar_base_height < 1) bar_base_height = 25;
                break;
            case 707:
                parse_color(bar_base_color);
                break;
            case 708:
                opt = atoi(optarg);
                if (opt > 0)
                    bar_periodic_step = opt;
                break;
            case 709:
                arg = optarg;
                if (sscanf(arg, "%31[^:]:%31[^:]", bar_x_expr, bar_y_expr) < 1) {
                    apierr("bar-position must be a single number or of the form x:y with a max length of 31\n");
                }
                break;
            case 710:
                bar_count = atoi(optarg);
                if (bar_count > MAX_BAR_COUNT || bar_count < MIN_BAR_COUNT) {
                    apierr("bar-count must be between %d and %d\n", MIN_BAR_COUNT, MAX_BAR_COUNT);
                }
                break;
            case 711:
                arg = optarg;
                if (sscanf(arg, "%31s", bar_width_expr) != 1) {
                    apierr("missing argument for bar-total-width\n");
                }
                break;

            case 998:
                image_raw_format = strdup(optarg);
                break;
            default:
                break;
        }
    }

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

static void init_str(str_t *str, size_t size) {
    str->value = (char*) malloc(size);
    memset(str->value, 0, size);
    str->size = size;
}
static char** str_destruct(arr_t arr) {
    size_t str_size = sizeof(str_t) * arr.size;
    char **str = (char**) malloc(sizeof(str_t) * arr.size);
    memset(str, 0, str_size);

    for (int i = 0; i < arr.size; i++) {
        str[i] = strdup(arr.items[i].value);
    }
    return str;
}
static void str_push_c(str_t *str, char c) {
    int len_str = strlen(str->value);
    if (len_str + 2 >= str->size) {
        str->size = (len_str + 1) * 2;
        str->value = (char *) realloc(str->value, str->size);
    }
    str->value[len_str] = c;
    str->value[len_str+1] = '\0';
}

static void push_back(arr_t *arr, str_t str) {
    arr->items = (str_t *) realloc(arr->items, sizeof(str_t) * (arr->size + 2));

    str_t toadd;
    init_str(&toadd, 0);
    toadd.value = strdup(str.value);
    toadd.size = str.size;

    arr->items[arr->size] = toadd;
    arr->size++;
}

static void init_str_arr(arr_t *arr) {
    str_t null;
    init_str(&null, 0);
    arr->items = (str_t *) malloc(0);
    arr->items[0] = null;
    arr->size = 0;
}

/**
 * Arguments parser based on "stringtoargcargv.cpp" by Bernhard Eder
 * @see https://web.archive.org/web/20121030075237/http://bbgen.net/blog/2011/06/string-to-argc-argv
 */
static int parse_args(arg_data *data, char str[]) {
    char c;
    int i = 0;
    str_t curr_arg;
    arr_t out_args;
    init_str(&curr_arg, 20);
    init_str_arr(&out_args);
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
                    free(curr_arg.value);
                    init_str(&curr_arg, 20);
                case InArg:
                    curr_state = InArgQuote;
                    curr_quote = c;
                    break;
                case InArgQuote:
                    if (c == curr_quote)
                        curr_state = InArg;
                    else
                        str_push_c(&curr_arg, c);
                    break;
            }
        } else
        if (c == ' ' || c == '\t') { // Whitespace
            switch(curr_state) {
                case InArg:
                    push_back(&out_args, curr_arg);
                    curr_state = OutOfArg;
                    break;
                case InArgQuote:
                    str_push_c(&curr_arg, c);
                    break;
                case OutOfArg:
                    break;
            }
        } else {
            switch(curr_state) {
                case InArg:
                case InArgQuote:
                    str_push_c(&curr_arg, c);
                    break;
                case OutOfArg:
                    free(curr_arg.value);
                    init_str(&curr_arg, 20);
                    str_push_c(&curr_arg, c);
                    curr_state = InArg;
                    break;
            }
        };
        i++;
    }

    if (curr_state == InArg) {
        push_back(&out_args, curr_arg);
    } else if (curr_state == InArgQuote) {
        return 1;
    }

    size_t argc = out_args.size;
    char **argv = str_destruct(out_args);

    free(curr_arg.value);
    free(out_args.items);

    data->argc = argc;
    data->argv = argv;

    return 0;
}

void *listen_api(void* _) {

    if (access(api_fifo_path, F_OK) == 0) {
        if (remove(api_fifo_path) != 0) {
            fprintf(stderr, "Could not remove file \"%s\": %s\n",
                    api_fifo_path, strerror(errno));
        }
    }
    if (mkfifo(api_fifo_path, S_IRWXU) != 0) {
        fprintf(stderr, "Could not create FIFO file \"%s\": %s\n",
                api_fifo_path, strerror(errno));
    }

    FILE *fifo;
    char c;
    int i;
    int times = 0;
    char *text = (char *) malloc(50);
    arg_data data;

    DEBUG("Created fifo at \"%s\"\n", api_fifo_path);

    while (1) {
        strcpy(text, "command ");
        i = strlen(text);

        fifo = fopen(api_fifo_path, "rb");
        if (fifo == NULL) {
            fprintf(stderr, "Could not open FIFO file \"%s\": %s\n",
                    api_fifo_path, strerror(errno));
            break;
        }
        while ((c = fgetc(fifo)) != EOF) {
            if (sizeof(text) <= i) {
                text = (char *) realloc(text, i * 2);
            }
            if (c == '\n') {
                text[i] = ' ';
            } else {
                text[i] = c;
            }
            i++;
        }
        text[i] = '\0';
        fclose(fifo);

        DEBUG("API message received - %d\n", times);
        DEBUG("Message: \"%s\"\n", text);
        times++;
        if (parse_args(&data, text) != 0) {
            fprintf(stderr, "Couldn not parse message\n");
            continue;
        }

        update_arguments(data.argc, data.argv);
        init_colors_once();

        free(text);
        text = (char *) malloc(50);
        /*redraw_screen();*/
    }

    return NULL;
}
