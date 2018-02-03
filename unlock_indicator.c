/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2010 Michael Stapelberg
 *
 * See LICENSE for licensing information
 *
 */
#include "unlock_indicator.h"

#include <cairo.h>
#include <cairo/cairo-xcb.h>
#include <ev.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fonts.h"
#include "i3lock.h"
#include "randr.h"
#include "tinyexpr.h"
#include "xcb.h"

/* clock stuff */
#include <time.h>

extern double circle_radius;
extern double ring_width;

#define BUTTON_RADIUS (circle_radius)
#define RING_WIDTH (ring_width)
#define BUTTON_SPACE (BUTTON_RADIUS + (RING_WIDTH / 2))
#define BUTTON_DIAMETER (2 * BUTTON_SPACE)

/*******************************************************************************
 * Variables defined in i3lock.c.
 ******************************************************************************/

extern bool debug_mode;

/* The current position in the input buffer. Useful to determine if any
 * characters of the password have already been entered or not. */
extern int input_position;

/* The lock window. */
extern xcb_window_t win;

/* The current resolution of the X11 root window. */
extern uint32_t last_resolution[2];

/* Whether the unlock indicator is enabled (defaults to true). */
extern bool unlock_indicator;

/* List of pressed modifiers, or NULL if none are pressed. */
extern char *modifier_string;

/* A Cairo surface containing the specified image (-i), if any. */
extern cairo_surface_t *img;
extern cairo_surface_t *blur_img;

/* Whether the image should be tiled. */
extern bool tile;
/* The background color to use (in hex). */
extern char color[7];
/* indicator color options */
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
extern char keyhlcolor[9];
extern char bshlcolor[9];
extern char separatorcolor[9];
extern int internal_line_source;

extern int screen_number;
extern float refresh_rate;

extern bool show_clock;
extern bool always_show_clock;
extern bool show_indicator;
extern int verif_align;
extern int wrong_align;
extern int time_align;
extern int date_align;
extern int layout_align;
extern int modif_align;
extern char time_format[32];
extern char date_format[32];
extern char *fonts[5];
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

extern double time_size;
extern double date_size;
extern double verif_size;
extern double wrong_size;
extern double modifier_size;
extern double layout_size;

extern char *verif_text;
extern char *wrong_text;
extern char *layout_text;

/* Whether the failed attempts should be displayed. */
extern bool show_failed_attempts;
/* Number of failed unlock attempts. */
extern int failed_attempts;

/*******************************************************************************
 * Variables defined in xcb.c.
 ******************************************************************************/

/* The root screen, to determine the DPI. */
extern xcb_screen_t *screen;

/*******************************************************************************
 * Local variables.
 ******************************************************************************/

/* time stuff */
static struct ev_periodic *time_redraw_tick;

/* Cache the screen’s visual, necessary for creating a Cairo context. */
static xcb_visualtype_t *vistype;

/* Maintain the current unlock/PAM state to draw the appropriate unlock
 * indicator. */
unlock_state_t unlock_state;
auth_state_t auth_state;

// color arrays
rgba_t insidever16;
rgba_t insidewrong16;
rgba_t inside16;
rgba_t ringver16;
rgba_t ringwrong16;
rgba_t ring16;
rgba_t line16;
rgba_t verif16;
rgba_t wrong16;
rgba_t layout16;
rgba_t time16;
rgba_t date16;
rgba_t keyhl16;
rgba_t bshl16;
rgba_t sep16;
rgba_t bar16;
// just rgb
rgb_t rgb16;

// experimental bar stuff

#define BAR_VERT 0
#define BAR_FLAT 1
// experimental bar stuff
extern bool bar_enabled;
extern double *bar_heights;
extern double bar_step;
extern double bar_base_height;
extern double bar_periodic_step;
extern double max_bar_height;
extern double bar_position;
extern int num_bars;
extern int bar_width;
extern int bar_orientation;

extern char bar_base_color[9];
extern char bar_expr[32];
extern bool bar_bidirectional;
extern bool bar_reversed;

static cairo_font_face_t *font_faces[5] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

/*
 * Returns the scaling factor of the current screen. E.g., on a 227 DPI MacBook
 * Pro 13" Retina screen, the scaling factor is 227/96 = 2.36.
 *
 */
static double calculate_scaling_factor(void) {
    const double dpi = (double)screen->height_in_pixels * 25.4 /
                       (double)screen->height_in_millimeters;
    return dpi / 96.0;
}

static cairo_font_face_t *get_font_face(int which) {
    if (font_faces[which]) {
        return font_faces[which];
    }
    const unsigned char *face_name = (const unsigned char *)fonts[which];
    FcResult result;
    /*
     * Loads the default config.
     * On successive calls, does no work and just returns true.
     */
    if (!FcInit()) {
        DEBUG("Fontconfig init failed. No text will be shown.\n");
        return NULL;
    }

    /*
     * converts a font face name to a pattern for that face name
     */
    FcPattern *pattern = FcNameParse(face_name);
    if (!pattern) {
        DEBUG("no sans-serif font available\n");
        return NULL;
    }

    /*
     * Gets the default font for our pattern. (Gets the default sans-serif font face)
     * Without these two calls, the FcFontMatch call will fail due to FcConfigGetCurrent()
     * not giving it a valid/useful config.
     */
    FcDefaultSubstitute(pattern);
    if (!FcConfigSubstitute(FcConfigGetCurrent(), pattern, FcMatchPattern)) {
        DEBUG("config sub failed?\n");
        return NULL;
    }

    /*
     * Looks up the font pattern and does some internal RenderPrepare work,
     * then returns the resulting pattern that's ready for rendering.
     */
    FcPattern *pattern_ready = FcFontMatch(FcConfigGetCurrent(), pattern, &result);
    FcPatternDestroy(pattern);
    pattern = NULL;
    if (!pattern_ready) {
        DEBUG("no sans-serif font available\n");
        return NULL;
    }

    /*
     * Passes the given pattern into cairo, which loads it into a cairo freetype font face.
     * Increment its reference count and cache it.
     */
    cairo_font_face_t *face = cairo_ft_font_face_create_for_pattern(pattern_ready);
    FcPatternDestroy(pattern_ready);
    font_faces[which] = cairo_font_face_reference(face);
    return face;
}

/*
 * Draws the given text onto the cairo context
 */
static void draw_text(cairo_t *ctx, text_t text) {
    if (!text.show)
        return;
    cairo_text_extents_t extents;

    cairo_set_font_face(ctx, text.font);
    cairo_set_font_size(ctx, text.size);

    cairo_text_extents(ctx, text.str, &extents);

    double x;

    switch (text.align) {
        case 1:
            x = text.x;
            break;
        case 2:
            x = text.x - (extents.width + extents.x_bearing);
            break;
        case 0:
        default:
            x = text.x - ((extents.width / 2) + (extents.x_bearing / 2));
            break;
    }

    cairo_set_source_rgba(ctx, text.color.red, text.color.green, text.color.blue, text.color.alpha);
    cairo_move_to(ctx, x, text.y);
    cairo_show_text(ctx, text.str);

    cairo_stroke(ctx);
}

static void draw_bar(cairo_t *ctx, double x, double y, double bar_offset) {
    // oh boy, here we go!
    // TODO: get this to play nicely with multiple monitors
    // ideally it'd intelligently span both monitors
    double width, height;
    double back_x = 0, back_y = 0, back_x2 = 0, back_y2 = 0, back_width = 0, back_height = 0;
    for (int i = 0; i < num_bars; ++i) {
        double cur_bar_height = bar_heights[i];

        if (cur_bar_height > 0) {
            if (unlock_state == STATE_BACKSPACE_ACTIVE) {
                cairo_set_source_rgba(ctx, bshl16.red, bshl16.green, bshl16.blue, bshl16.alpha);
            } else {
                cairo_set_source_rgba(ctx, keyhl16.red, keyhl16.green, keyhl16.blue, keyhl16.alpha);
            }
        } else {
            switch (auth_state) {
                case STATE_AUTH_VERIFY:
                case STATE_AUTH_LOCK:
                    cairo_set_source_rgba(ctx, ringver16.red, ringver16.green, ringver16.blue, ringver16.alpha);
                    break;
                case STATE_AUTH_WRONG:
                case STATE_I3LOCK_LOCK_FAILED:
                    cairo_set_source_rgba(ctx, ringwrong16.red, ringwrong16.green, ringwrong16.blue, ringwrong16.alpha);
                    break;
                default:
                    cairo_set_source_rgba(ctx, bar16.red, bar16.green, bar16.blue, bar16.alpha);
                    break;
            }
        }

        if (bar_orientation == BAR_VERT) {
            width = (cur_bar_height <= 0 ? bar_base_height : cur_bar_height);
            height = bar_width;
            x = bar_offset;
            y = i * bar_width;
            if (bar_reversed) {
                x -= width;
            } else if (bar_bidirectional) {
                width = (cur_bar_height <= 0 ? bar_base_height : cur_bar_height * 2);
                x = bar_offset - (width / 2) + (bar_base_height / 2);
            }
        } else {
            width = bar_width;
            height = (cur_bar_height <= 0 ? bar_base_height : cur_bar_height);
            x = i * bar_width;
            y = bar_offset;
            if (bar_reversed) {
                y -= height;
            } else if (bar_bidirectional) {
                height = (cur_bar_height <= 0 ? bar_base_height : cur_bar_height * 2);
                y = bar_offset - (height / 2) + (bar_base_height / 2);
            }
        }

        if (cur_bar_height < bar_base_height && cur_bar_height > 0) {
            if (bar_orientation == BAR_VERT) {
                back_x = bar_offset + cur_bar_height;
                back_y = y;
                back_width = bar_base_height - cur_bar_height;
                back_height = height;
                if (bar_reversed) {
                    back_x = bar_offset - bar_base_height;
                } else if (bar_bidirectional) {
                    back_x = bar_offset;
                    back_y2 = y;
                    back_width = (bar_base_height - (cur_bar_height * 2)) / 2;
                    back_x2 = bar_offset + (cur_bar_height * 2) + back_width;
                }
            } else {
                back_x = x;
                back_y = bar_offset + cur_bar_height;
                back_width = width;
                back_height = bar_base_height - cur_bar_height;
                if (bar_reversed) {
                    back_y = bar_offset - bar_base_height;
                } else if (bar_bidirectional) {
                    back_x2 = x;
                    back_y = bar_offset;
                    back_height = (bar_base_height - (cur_bar_height * 2)) / 2;
                    back_y2 = bar_offset + (cur_bar_height * 2) + back_height;
                }
            }
        }
        cairo_rectangle(ctx, x, y, width, height);
        cairo_fill(ctx);
        switch (auth_state) {
            case STATE_AUTH_VERIFY:
            case STATE_AUTH_LOCK:
                cairo_set_source_rgba(ctx, ringver16.red, ringver16.green, ringver16.blue, ringver16.alpha);
                break;
            case STATE_AUTH_WRONG:
            case STATE_I3LOCK_LOCK_FAILED:
                cairo_set_source_rgba(ctx, ringwrong16.red, ringwrong16.green, ringwrong16.blue, ringwrong16.alpha);
                break;
            default:
                cairo_set_source_rgba(ctx, bar16.red, bar16.green, bar16.blue, bar16.alpha);
                break;
        }

        if (cur_bar_height > 0 && cur_bar_height < bar_base_height && ((bar_bidirectional && ((cur_bar_height * 2) < bar_base_height)) || (!bar_bidirectional && (cur_bar_height < bar_base_height)))) {
            cairo_rectangle(ctx, back_x, back_y, back_width, back_height);
            cairo_fill(ctx);
            if (bar_bidirectional) {
                cairo_rectangle(ctx, back_x2, back_y2, back_width, back_height);
                cairo_fill(ctx);
            }
        }
    }
    for (int i = 0; i < num_bars; ++i) {
        if (bar_heights[i] > 0)
            bar_heights[i] -= bar_periodic_step;
    }
}

static void draw_indic(cairo_t *ctx, double ind_x, double ind_y) {
    if (unlock_indicator &&
        (unlock_state >= STATE_KEY_PRESSED || auth_state > STATE_AUTH_IDLE || show_indicator)) {
        /* Draw a (centered) circle with transparent background. */
        cairo_set_line_width(ctx, RING_WIDTH);
        cairo_arc(ctx, ind_x, ind_y, BUTTON_RADIUS, 0, 2 * M_PI);

        /* Use the appropriate color for the different PAM states
         * (currently verifying, wrong password, or default) */
        switch (auth_state) {
            case STATE_AUTH_VERIFY:
            case STATE_AUTH_LOCK:
                cairo_set_source_rgba(ctx, insidever16.red, insidever16.green, insidever16.blue, insidever16.alpha);
                break;
            case STATE_AUTH_WRONG:
            case STATE_I3LOCK_LOCK_FAILED:
                cairo_set_source_rgba(ctx, insidewrong16.red, insidewrong16.green, insidewrong16.blue, insidewrong16.alpha);
                break;
            default:
                if (unlock_state == STATE_NOTHING_TO_DELETE) {
                    cairo_set_source_rgba(ctx, insidewrong16.red, insidewrong16.green, insidewrong16.blue, insidewrong16.alpha);
                    break;
                }
                cairo_set_source_rgba(ctx, inside16.red, inside16.green, inside16.blue, inside16.alpha);
                break;
        }
        cairo_fill_preserve(ctx);

        switch (auth_state) {
            case STATE_AUTH_VERIFY:
            case STATE_AUTH_LOCK:
                cairo_set_source_rgba(ctx, ringver16.red, ringver16.green, ringver16.blue, ringver16.alpha);
                if (internal_line_source == 1) {
                    line16.red = ringver16.red;
                    line16.green = ringver16.green;
                    line16.blue = ringver16.blue;
                    line16.alpha = ringver16.alpha;
                }
                break;
            case STATE_AUTH_WRONG:
            case STATE_I3LOCK_LOCK_FAILED:
                cairo_set_source_rgba(ctx, ringwrong16.red, ringwrong16.green, ringwrong16.blue, ringwrong16.alpha);
                if (internal_line_source == 1) {
                    line16.red = ringwrong16.red;
                    line16.green = ringwrong16.green;
                    line16.blue = ringwrong16.blue;
                    line16.alpha = ringwrong16.alpha;
                }
                break;
            case STATE_AUTH_IDLE:
                if (unlock_state == STATE_NOTHING_TO_DELETE) {
                    cairo_set_source_rgba(ctx, ringwrong16.red, ringwrong16.green, ringwrong16.blue, ringwrong16.alpha);
                    if (internal_line_source == 1) {
                        line16.red = ringwrong16.red;
                        line16.green = ringwrong16.green;
                        line16.blue = ringwrong16.blue;
                        line16.alpha = ringwrong16.alpha;
                    }
                    break;
                }
                cairo_set_source_rgba(ctx, ring16.red, ring16.green, ring16.blue, ring16.alpha);
                if (internal_line_source == 1) {
                    line16.red = ring16.red;
                    line16.green = ring16.green;
                    line16.blue = ring16.blue;
                    line16.alpha = ring16.alpha;
                }
                break;
        }
        cairo_stroke(ctx);

        /* Draw an inner separator line. */
        if (internal_line_source != 2) {  //pretty sure this only needs drawn if it's being drawn over the inside?
            cairo_set_source_rgba(ctx, line16.red, line16.green, line16.blue, line16.alpha);
            cairo_set_line_width(ctx, 2.0);
            cairo_arc(ctx, ind_x, ind_y, BUTTON_RADIUS - 5, 0, 2 * M_PI);
            cairo_stroke(ctx);
        }
        if (unlock_state == STATE_KEY_ACTIVE || unlock_state == STATE_BACKSPACE_ACTIVE) {
            cairo_set_line_width(ctx, RING_WIDTH);
            cairo_new_sub_path(ctx);
            double highlight_start = (rand() % (int)(2 * M_PI * 100)) / 100.0;
            cairo_arc(ctx, ind_x, ind_y, BUTTON_RADIUS,
                      highlight_start, highlight_start + (M_PI / 3.0));
            if (unlock_state == STATE_KEY_ACTIVE) {
                /* For normal keys, we use a lighter green. */
                cairo_set_source_rgba(ctx, keyhl16.red, keyhl16.green, keyhl16.blue, keyhl16.alpha);
            } else {
                /* For backspace, we use red. */
                cairo_set_source_rgba(ctx, bshl16.red, bshl16.green, bshl16.blue, bshl16.alpha);
            }

            cairo_stroke(ctx);

            /* Draw two little separators for the highlighted part of the
             * unlock indicator. */
            cairo_set_source_rgba(ctx, sep16.red, sep16.green, sep16.blue, sep16.alpha);
            cairo_arc(ctx, ind_x, ind_y, BUTTON_RADIUS,
                      highlight_start, highlight_start + (M_PI / 128.0));
            cairo_stroke(ctx);
            cairo_arc(ctx, ind_x, ind_y, BUTTON_RADIUS,
                      (highlight_start + (M_PI / 3.0)) - (M_PI / 128.0),
                      highlight_start + (M_PI / 3.0));
            cairo_stroke(ctx);
        }
    }
}

/*
 * Initialize all the color arrays once.
 * Called once after options are parsed.
 */

/*
    colorstring: 8-character RGBA string ("ff0000ff", "00000000", "ffffffff", etc)
    colorstring16: array of 4 integers (r, g, b, a).
    MAKE_COLORGROUPS(colorstring, colorstring16) =>

    char colorstring_tmparr[4][3] = {{colorstring[0], colorstring[1], '\0'},
                                     {colorstring[2], colorstring[3], '\0'},
                                     {colorstring[4], colorstring[5], '\0'},
                                     {colorstring[6], colorstring[7], '\0'}};
    uint32_t colorstring16[4] = {(strtol(colorstring_tmparr[0], NULL, 16)),
                                 (strtol(colorstring_tmparr[1], NULL, 16)),
                                 (strtol(colorstring_tmparr[2], NULL, 16)),
                                 (strtol(colorstring_tmparr[3], NULL, 16))};
 */

static void set_color(char *dest, const char *src, int offset) {
    dest[0] = src[offset];
    dest[1] = src[offset + 1];
    dest[2] = '\0';
}

static void colorgen(rgba_str_t *tmp, const char *src, rgba_t *dest) {
    set_color(tmp->red, src, 0);
    set_color(tmp->green, src, 2);
    set_color(tmp->blue, src, 4);
    set_color(tmp->alpha, src, 6);

    dest->red = strtol(tmp->red, NULL, 16) / 255.0;
    dest->green = strtol(tmp->green, NULL, 16) / 255.0;
    dest->blue = strtol(tmp->blue, NULL, 16) / 255.0;
    dest->alpha = strtol(tmp->alpha, NULL, 16) / 255.0;
}

static void colorgen_rgb(rgb_str_t *tmp, const char *src, rgb_t *dest) {
    set_color(tmp->red, src, 0);
    set_color(tmp->green, src, 2);
    set_color(tmp->blue, src, 4);

    dest->red = strtol(tmp->red, NULL, 16) / 255.0;
    dest->green = strtol(tmp->green, NULL, 16) / 255.0;
    dest->blue = strtol(tmp->blue, NULL, 16) / 255.0;
}

void init_colors_once(void) {
    rgba_str_t tmp;
    rgb_str_t tmp_rgb;

    /* build indicator color arrays */
    colorgen(&tmp, insidevercolor, &insidever16);
    colorgen(&tmp, insidewrongcolor, &insidewrong16);
    colorgen(&tmp, insidecolor, &inside16);
    colorgen(&tmp, ringvercolor, &ringver16);
    colorgen(&tmp, ringwrongcolor, &ringwrong16);
    colorgen(&tmp, ringcolor, &ring16);
    colorgen(&tmp, linecolor, &line16);
    colorgen(&tmp, verifcolor, &verif16);
    colorgen(&tmp, wrongcolor, &wrong16);
    colorgen(&tmp, layoutcolor, &layout16);
    colorgen(&tmp, timecolor, &time16);
    colorgen(&tmp, datecolor, &date16);
    colorgen(&tmp, keyhlcolor, &keyhl16);
    colorgen(&tmp, bshlcolor, &bshl16);
    colorgen(&tmp, separatorcolor, &sep16);
    colorgen(&tmp, bar_base_color, &bar16);
    colorgen_rgb(&tmp_rgb, color, &rgb16);
}

te_expr *compile_expression(const char *const from, const char *expression, const te_variable *variables, int var_count);
te_expr *compile_expression(const char *const from, const char *expression, const te_variable *variables, int var_count) {
    int te_err = 0;
    te_expr *expr = te_compile(expression, variables, var_count, &te_err);
    if (te_err) {
        fprintf(stderr, "Failed to reason about '%s' given by '%s'\n", expression, from);
        exit(1);
    }
    return expr;
}

/*
 * Draws global image with fill color onto a pixmap with the given
 * resolution and returns it.
 *
 */
xcb_pixmap_t draw_image(uint32_t *resolution) {
    const double scaling_factor = calculate_scaling_factor();
    xcb_pixmap_t bg_pixmap = XCB_NONE;

    if (!vistype)
        vistype = get_root_visual_type(screen);
    bg_pixmap = create_bg_pixmap(conn, screen, resolution, color);
    /* Initialize cairo: Create one in-memory surface to render the unlock
     * indicator on, create one XCB surface to actually draw (one or more,
     * depending on the amount of screens) unlock indicators on.
     * create two more surfaces for time and date display
     */
    cairo_surface_t *output = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, resolution[0], resolution[1]);
    cairo_t *ctx = cairo_create(output);
    cairo_scale(ctx, scaling_factor, scaling_factor);

    //    cairo_set_font_face(ctx, get_font_face(0));

    cairo_surface_t *xcb_output = cairo_xcb_surface_create(conn, bg_pixmap, vistype, resolution[0], resolution[1]);
    cairo_t *xcb_ctx = cairo_create(xcb_output);

    if (blur_img || img) {
        if (blur_img) {
            cairo_set_source_surface(xcb_ctx, blur_img, 0, 0);
            cairo_paint(xcb_ctx);
        } else {  // img can no longer be non-NULL if blur_img is not null
            if (!tile) {
                cairo_set_source_surface(xcb_ctx, img, 0, 0);
                cairo_paint(xcb_ctx);
            } else {
                /* create a pattern and fill a rectangle as big as the screen */
                cairo_pattern_t *pattern;
                pattern = cairo_pattern_create_for_surface(img);
                cairo_set_source(xcb_ctx, pattern);
                cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
                cairo_rectangle(xcb_ctx, 0, 0, resolution[0], resolution[1]);
                cairo_fill(xcb_ctx);
                cairo_pattern_destroy(pattern);
            }
        }
    } else {
        cairo_set_source_rgb(xcb_ctx, rgb16.red, rgb16.green, rgb16.blue);
        cairo_rectangle(xcb_ctx, 0, 0, resolution[0], resolution[1]);
        cairo_fill(xcb_ctx);
    }

    /*
     * gen text
     * calc vars
     * process if keystroke or not
     * draw indicator
     * draw text
     */

    // generate all the text to render
    text_t status_text;
    status_text.show = false;
    status_text.font = NULL;

    text_t mod_text;
    mod_text.show = false;
    mod_text.font = NULL;

    text_t keylayout_text;
    keylayout_text.show = false;
    keylayout_text.font = NULL;

    text_t time_text;
    time_text.show = false;
    time_text.font = NULL;

    text_t date_text;
    date_text.show = false;
    date_text.font = NULL;

    if (unlock_indicator &&
        (unlock_state >= STATE_KEY_PRESSED || auth_state > STATE_AUTH_IDLE || show_indicator)) {
        switch (auth_state) {
            case STATE_AUTH_VERIFY:
                status_text.show = true;
                strncpy(status_text.str, verif_text, sizeof(status_text.str));
                status_text.font = get_font_face(VERIF_FONT);
                status_text.color = verif16;
                status_text.size = verif_size;
                status_text.align = verif_align;
                break;
            case STATE_AUTH_LOCK:
                status_text.show = true;
                strncpy(status_text.str, "locking…", sizeof(status_text.str));
                status_text.font = get_font_face(VERIF_FONT);
                status_text.color = verif16;
                status_text.size = verif_size;
                status_text.align = verif_align;
                break;
            case STATE_AUTH_WRONG:
                status_text.show = true;
                strncpy(status_text.str, wrong_text, sizeof(status_text.str));
                status_text.font = get_font_face(WRONG_FONT);
                status_text.color = wrong16;
                status_text.size = wrong_size;
                status_text.align = wrong_align;
                break;
            case STATE_I3LOCK_LOCK_FAILED:
                status_text.show = true;
                strncpy(status_text.str, "lock failed!", sizeof(status_text.str));
                status_text.font = get_font_face(WRONG_FONT);
                status_text.color = wrong16;
                status_text.size = wrong_size;
                status_text.align = wrong_align;
                break;
            default:
                if (unlock_state == STATE_NOTHING_TO_DELETE) {
                    status_text.show = true;
                    strncpy(status_text.str, "no input", sizeof(status_text.str));
                    status_text.font = get_font_face(WRONG_FONT);
                    status_text.color = wrong16;
                    status_text.size = wrong_size;
                    status_text.align = wrong_align;
                    break;
                }
                if (show_failed_attempts && failed_attempts > 0) {
                    status_text.show = true;
                    status_text.font = get_font_face(WRONG_FONT);
                    status_text.color = wrong16;
                    status_text.size = wrong_size;
                    status_text.align = wrong_align;
                    // TODO: variable for this
                    status_text.size = 32.0;
                    if (failed_attempts > 999) {
                        strncpy(status_text.str, "> 999", sizeof(status_text.str));
                    } else {
                        snprintf(status_text.str, sizeof(status_text.str), "%d", failed_attempts);
                    }
                }
                break;
        }
    }

    if (modifier_string) {
        mod_text.show = true;
        strncpy(mod_text.str, modifier_string, sizeof(mod_text.str));
        mod_text.size = modifier_size;
        mod_text.font = get_font_face(WRONG_FONT);
        mod_text.align = modif_align;

        mod_text.color = wrong16;
    }

    if (layout_text) {
        keylayout_text.show = true;
        strncpy(keylayout_text.str, layout_text, sizeof(mod_text.str));
        keylayout_text.size = layout_size;
        keylayout_text.font = get_font_face(LAYOUT_FONT);
        keylayout_text.color = layout16;
        keylayout_text.align = layout_align;
    }

    if (show_clock && (!status_text.show || always_show_clock)) {
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(time_text.str, 40, time_format, timeinfo);
        if (*time_text.str) {
            time_text.show = true;
            time_text.size = time_size;
            time_text.color = time16;
            time_text.font = get_font_face(TIME_FONT);
            time_text.align = time_align;
        }
        strftime(date_text.str, 40, date_format, timeinfo);
        if (*date_text.str) {
            date_text.show = true;
            date_text.size = date_size;
            date_text.color = date16;
            date_text.font = get_font_face(DATE_FONT);
            date_text.align = date_align;
        }
    }

    // initialize positioning vars
    double screen_x = 0, screen_y = 0,
           width = 0, height = 0;

    double mod_x = 0, mod_y = 0,
           date_x = 0, date_y = 0,
           bar_x = 0, bar_y = 0,
           layout_x = 0, layout_y = 0,
           status_x = 0, status_y = 0,
           time_x = 0, time_y = 0,
           indicator_x = 0, indicator_y = 0;

    double radius = (circle_radius + ring_width);
    double bar_offset = 0;
    int button_diameter_physical = ceil(scaling_factor * BUTTON_DIAMETER);
    DEBUG("scaling_factor is %.f, physical diameter is %d px\n",
          scaling_factor, button_diameter_physical);

    // variable mapping for evaluating the clock position expression
    const unsigned int vars_size = 11;
    te_variable vars[] =
        {{"w", &width},
         {"h", &height},
         {"x", &screen_x},
         {"y", &screen_y},
         {"ix", &indicator_x},
         {"iy", &indicator_y},
         {"tx", &time_x},
         {"ty", &time_y},
         {"dx", &date_x},
         {"dy", &date_y},
         {"r", &radius}};

    te_expr *te_ind_x_expr = compile_expression("--indpos", ind_x_expr, vars, vars_size);
    te_expr *te_ind_y_expr = compile_expression("--indpos", ind_y_expr, vars, vars_size);
    te_expr *te_time_x_expr = compile_expression("--timepos", time_x_expr, vars, vars_size);
    te_expr *te_time_y_expr = compile_expression("--timepos", time_y_expr, vars, vars_size);
    te_expr *te_date_x_expr = compile_expression("--datepos", date_x_expr, vars, vars_size);
    te_expr *te_date_y_expr = compile_expression("--datepos", date_y_expr, vars, vars_size);
    te_expr *te_layout_x_expr = compile_expression("--layoutpos", layout_x_expr, vars, vars_size);
    te_expr *te_layout_y_expr = compile_expression("--layoutpos", layout_y_expr, vars, vars_size);
    te_expr *te_status_x_expr = compile_expression("--statuspos", status_x_expr, vars, vars_size);
    te_expr *te_status_y_expr = compile_expression("--statuspos", status_y_expr, vars, vars_size);
    te_expr *te_modif_x_expr = compile_expression("--modifpos", modif_x_expr, vars, vars_size);
    te_expr *te_modif_y_expr = compile_expression("--modifpos", modif_y_expr, vars, vars_size);
    te_expr *te_bar_expr = compile_expression("--bar-position", bar_expr, vars, vars_size);

    if (screen_number < 0 || !(screen_number < xr_screens)) {
        screen_number = 0;
    }

    if (xr_screens > 0 && !bar_enabled) {
        width = xr_resolutions[screen_number].width / scaling_factor;
        height = xr_resolutions[screen_number].height / scaling_factor;
        screen_x = xr_resolutions[screen_number].x / scaling_factor;
        screen_y = xr_resolutions[screen_number].y / scaling_factor;
        if (te_ind_x_expr && te_ind_y_expr) {
            indicator_x = te_eval(te_ind_x_expr);
            indicator_y = te_eval(te_ind_y_expr);
        } else {
            indicator_x = screen_x + (width / 2);
            indicator_y = screen_y + (height / 2);
        }
        bar_x = indicator_x - (button_diameter_physical / 2);
        bar_y = indicator_y - (button_diameter_physical / 2);
        time_x = te_eval(te_time_x_expr);
        time_y = te_eval(te_time_y_expr);
        date_x = te_eval(te_date_x_expr);
        date_y = te_eval(te_date_y_expr);
        layout_x = te_eval(te_layout_x_expr);
        layout_y = te_eval(te_layout_y_expr);
        status_x = te_eval(te_status_x_expr);
        status_y = te_eval(te_status_y_expr);
        mod_x = te_eval(te_modif_x_expr);
        mod_y = te_eval(te_modif_y_expr);
    } else if (!bar_enabled) {
        /* We have no information about the screen sizes/positions, so we just
         * place the unlock indicator in the middle of the X root window and
         * hope for the best. */
        width = last_resolution[0] / scaling_factor;
        height = last_resolution[1] / scaling_factor;
        indicator_x = width / 2;
        indicator_y = height / 2;
        bar_x = indicator_x - (button_diameter_physical / 2);
        bar_y = indicator_y - (button_diameter_physical / 2);
        if (te_time_x_expr && te_time_y_expr) {
            time_x = te_eval(te_time_x_expr);
            time_y = te_eval(te_time_y_expr);
            date_x = te_eval(te_date_x_expr);
            date_y = te_eval(te_date_y_expr);
            layout_x = te_eval(te_layout_x_expr);
            layout_y = te_eval(te_layout_y_expr);
            status_x = te_eval(te_status_x_expr);
            status_y = te_eval(te_status_y_expr);
            mod_x = te_eval(te_modif_x_expr);
            mod_y = te_eval(te_modif_y_expr);
        }
    } else {
        width = xr_resolutions[screen_number].width / scaling_factor;
        height = xr_resolutions[screen_number].height / scaling_factor;
        indicator_x = width / 2;
        indicator_y = height / 2;
        bar_x = indicator_x - (button_diameter_physical / 2);
        bar_y = indicator_y - (button_diameter_physical / 2);

        bar_offset = te_eval(te_bar_expr);
        time_x = te_eval(te_time_x_expr);
        time_y = te_eval(te_time_y_expr);
        date_x = te_eval(te_date_x_expr);
        date_y = te_eval(te_date_y_expr);
        layout_x = te_eval(te_layout_x_expr);
        layout_y = te_eval(te_layout_y_expr);
        status_x = te_eval(te_status_x_expr);
        status_y = te_eval(te_status_y_expr);
        mod_x = te_eval(te_modif_x_expr);
        mod_y = te_eval(te_modif_y_expr);
    }

    te_free(te_ind_x_expr);
    te_free(te_ind_y_expr);
    te_free(te_time_x_expr);
    te_free(te_time_y_expr);
    te_free(te_date_x_expr);
    te_free(te_date_y_expr);
    te_free(te_layout_x_expr);
    te_free(te_layout_y_expr);
    te_free(te_status_x_expr);
    te_free(te_status_y_expr);
    te_free(te_modif_x_expr);
    te_free(te_modif_y_expr);
    te_free(te_bar_expr);

    DEBUG("Indicator at %fx%f\n", indicator_x, indicator_y);
    DEBUG("Bar at %fx%f\n", bar_x, bar_y);
    DEBUG("Time at %fx%f\n", time_x, time_y);
    DEBUG("Date at %fx%f\n", date_x, date_y);
    DEBUG("Layout at %fx%f\n", layout_x, layout_y);
    DEBUG("Status at %fx%f\n", status_x, status_y);
    DEBUG("Mod at %fx%f\n", mod_x, mod_y);

    status_text.x = status_x;
    status_text.y = status_y;

    keylayout_text.x = layout_x;
    keylayout_text.y = layout_y;

    mod_text.x = mod_x;
    mod_text.y = mod_y;

    time_text.x = time_x;
    time_text.y = time_y;
    date_text.x = date_x;
    date_text.y = date_y;

    // indicator stuff
    if (!bar_enabled) {
        draw_indic(ctx, indicator_x, indicator_y);
    } else {
        if (unlock_state == STATE_KEY_ACTIVE ||
            unlock_state == STATE_BACKSPACE_ACTIVE) {
            // note: might be biased to cause more hits on lower indices
            // maybe see about doing ((double) rand() / RAND_MAX) * num_bars
            int index = rand() % num_bars;
            bar_heights[index] = max_bar_height;
            for (int i = 0; i < ((max_bar_height / bar_step) + 1); ++i) {
                int low_ind = index - i;
                while (low_ind < 0) {
                    low_ind += num_bars;
                }
                int high_ind = (index + i) % num_bars;
                int tmp_height = max_bar_height - (bar_step * i);
                if (tmp_height < 0)
                    tmp_height = 0;
                if (bar_heights[low_ind] < tmp_height)
                    bar_heights[low_ind] = tmp_height;
                if (bar_heights[high_ind] < tmp_height)
                    bar_heights[high_ind] = tmp_height;
                if (tmp_height == 0)
                    break;
            }
        }
        draw_bar(ctx, bar_x, bar_y, bar_offset);
    }

    draw_text(ctx, status_text);
    draw_text(ctx, keylayout_text);
    draw_text(ctx, mod_text);
    draw_text(ctx, time_text);
    draw_text(ctx, date_text);

    cairo_set_source_surface(xcb_ctx, output, 0, 0);
    cairo_rectangle(xcb_ctx, 0, 0, resolution[0], resolution[1]);
    cairo_fill(xcb_ctx);

    cairo_surface_destroy(xcb_output);
    cairo_surface_destroy(output);
    cairo_destroy(ctx);
    cairo_destroy(xcb_ctx);
    return bg_pixmap;
}

/*
 * Calls draw_image on a new pixmap and swaps that with the current pixmap
 *
 */
void redraw_screen(void) {
    DEBUG("redraw_screen(unlock_state = %d, auth_state = %d) @ [%lu]\n", unlock_state, auth_state, (unsigned long)time(NULL));
    xcb_pixmap_t bg_pixmap = draw_image(last_resolution);
    xcb_change_window_attributes(conn, win, XCB_CW_BACK_PIXMAP, (uint32_t[1]){bg_pixmap});
    /* XXX: Possible optimization: Only update the area in the middle of the
     * screen instead of the whole screen. */
    xcb_clear_area(conn, 0, win, 0, 0, last_resolution[0], last_resolution[1]);
    xcb_free_pixmap(conn, bg_pixmap);
    xcb_flush(conn);
}

/*
 * Hides the unlock indicator completely when there is no content in the
 * password buffer.
 *
 */
void clear_indicator(void) {
    if (input_position == 0) {
        unlock_state = STATE_STARTED;
    } else
        unlock_state = STATE_KEY_PRESSED;
    redraw_screen();
}

void *start_time_redraw_tick_pthread(void *arg) {
    struct timespec *ts = (struct timespec *)arg;
    while (1) {
        nanosleep(ts, NULL);
        redraw_screen();
    }
    return NULL;
}

static void time_redraw_cb(struct ev_loop *loop, ev_periodic *w, int revents) {
    redraw_screen();
}

void start_time_redraw_tick(struct ev_loop *main_loop) {
    if (time_redraw_tick) {
        ev_periodic_set(time_redraw_tick, 0., refresh_rate, 0);
        ev_periodic_again(main_loop, time_redraw_tick);
    } else {
        if (!(time_redraw_tick = calloc(sizeof(struct ev_periodic), 1))) {
            return;
        }
        ev_periodic_init(time_redraw_tick, time_redraw_cb, 0., refresh_rate, 0);
        ev_periodic_start(main_loop, time_redraw_tick);
    }
}
