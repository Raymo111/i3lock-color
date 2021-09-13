/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2010 Michael Stapelberg
 * © 2015 Cassandra Fox
 * © 2021 Raymond Li
 *
 * See LICENSE for licensing information
 *
 */
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <ev.h>
#include <cairo.h>
#include <cairo/cairo-xcb.h>

#include "i3lock.h"
#include "xcb.h"
#include "unlock_indicator.h"
#include "randr.h"
#include "dpi.h"
#include "tinyexpr.h"
#include "fonts.h"

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
extern cairo_surface_t *img_slideshow[256];
extern cairo_surface_t *blur_bg_img;
extern int slideshow_image_count;
extern int slideshow_interval;
extern bool slideshow_random_selection;

unsigned long lastCheck;

/* How the background image should be displayed */
extern background_type_t bg_type;
/* The background color to use (in hex). */
extern char color[9];
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
extern char modifcolor[9];
extern char keyhlcolor[9];
extern char bshlcolor[9];
extern char separatorcolor[9];
extern char greetercolor[9];
extern int internal_line_source;

extern char verifoutlinecolor[9];
extern char wrongoutlinecolor[9];
extern char layoutoutlinecolor[9];
extern char timeoutlinecolor[9];
extern char dateoutlinecolor[9];
extern char greeteroutlinecolor[9];
extern char modifoutlinecolor[9];

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
extern int greeter_align;
extern char time_format[32];
extern char date_format[32];
extern char *fonts[6];
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
extern char verif_x_expr[32];
extern char verif_y_expr[32];
extern char wrong_x_expr[32];
extern char wrong_y_expr[32];
extern char modif_x_expr[32];
extern char modif_y_expr[32];
extern char greeter_x_expr[32];
extern char greeter_y_expr[32];

extern double time_size;
extern double date_size;
extern double verif_size;
extern double wrong_size;
extern double modifier_size;
extern double layout_size;
extern double greeter_size;

extern double timeoutlinewidth;
extern double dateoutlinewidth;
extern double verifoutlinewidth;
extern double wrongoutlinewidth;
extern double modifieroutlinewidth;
extern double layoutoutlinewidth;
extern double greeteroutlinewidth;

extern char *verif_text;
extern char *wrong_text;
extern char *noinput_text;
extern char *lock_text;
extern char *lock_failed_text;
extern char *layout_text;
extern char *greeter_text;

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

int current_slideshow_index = 0;

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
rgba_t modif16;
rgba_t keyhl16;
rgba_t bshl16;
rgba_t sep16;
rgba_t bar16;
rgba_t greeter16;
rgba_t background;

rgba_t verifoutline16;
rgba_t wrongoutline16;
rgba_t layoutoutline16;
rgba_t timeoutline16;
rgba_t dateoutline16;
rgba_t greeteroutline16;
rgba_t modifoutline16;

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
extern int bar_count;
extern int bar_orientation;

extern char bar_base_color[9];
extern char bar_x_expr[32];
extern char bar_y_expr[32];
extern char bar_width_expr[32];
extern bool bar_bidirectional;
extern bool bar_reversed;

static cairo_font_face_t *font_faces[6] = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

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
    FcFini();
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
            x = text.x - extents.x_advance / 2;
            break;
    }

    cairo_set_source_rgba(ctx, text.color.red, text.color.green, text.color.blue, text.color.alpha);
    cairo_move_to(ctx, x, text.y);

    cairo_text_path(ctx, text.str);
    cairo_fill_preserve(ctx);

    cairo_set_source_rgba(ctx, text.outline_color.red, text.outline_color.green, text.outline_color.blue, text.outline_color.alpha);
    cairo_set_line_width(ctx, text.outline_width);
    cairo_stroke(ctx);
}

static void draw_single_bar(cairo_t *ctx, double pos, double offset, double width, double height) {
    if (bar_reversed) {
        offset -= height;
    } else if (bar_bidirectional) {
        offset -= height / 2;
    }

    if (bar_orientation == BAR_VERT)
        cairo_rectangle(ctx, offset, pos, height, width);
    else
        cairo_rectangle(ctx, pos, offset, width, height);
    cairo_fill(ctx);
}

static void draw_bar(cairo_t *ctx, double bar_x, double bar_y, double bar_width, double screen_x, double screen_y) {

    cairo_save(ctx);

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

    if (bar_orientation == BAR_VERT)
        draw_single_bar(ctx, bar_y, bar_x, bar_width, bar_base_height);
    else
        draw_single_bar(ctx, bar_x, bar_y, bar_width, bar_base_height);

    if (unlock_state == STATE_BACKSPACE_ACTIVE)
        cairo_set_source_rgba(ctx, bshl16.red, bshl16.green, bshl16.blue, bshl16.alpha);
    else
        cairo_set_source_rgba(ctx, keyhl16.red, keyhl16.green, keyhl16.blue, keyhl16.alpha);

    cairo_set_operator(ctx, CAIRO_OPERATOR_SOURCE);

    double base_width = bar_width / bar_count;
    double bar_pos, bar_offset;
    if (bar_orientation == BAR_VERT) {
        bar_pos = bar_y;
        bar_offset = bar_x;
    } else {
        bar_pos = bar_x;
        bar_offset = bar_y;
    }

    for (int i = 0; i < bar_count; ++i) {
        double bar_height = bar_heights[i];
        if (bar_bidirectional) bar_height *= 2;
        if (bar_height > 0) {
            draw_single_bar(ctx, bar_pos + i * base_width, bar_offset, base_width, bar_height);
        }
    }

    for (int i = 0; i < bar_count; ++i) {
        if (bar_heights[i] > 0) {
            bar_heights[i] -= bar_periodic_step;
        }
    }

    cairo_restore(ctx);
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

void init_colors_once(void) {

    /* initialize for slideshow time interval */
    lastCheck = (unsigned long)time(NULL);

    rgba_str_t tmp;

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
    colorgen(&tmp, modifcolor, &modif16);
    colorgen(&tmp, keyhlcolor, &keyhl16);
    colorgen(&tmp, bshlcolor, &bshl16);
    colorgen(&tmp, separatorcolor, &sep16);
    colorgen(&tmp, bar_base_color, &bar16);
    colorgen(&tmp, greetercolor, &greeter16);
    colorgen(&tmp, color, &background);

    colorgen(&tmp, verifoutlinecolor, &verifoutline16);
    colorgen(&tmp, wrongoutlinecolor, &wrongoutline16);
    colorgen(&tmp, layoutoutlinecolor, &layoutoutline16);
    colorgen(&tmp, timeoutlinecolor, &timeoutline16);
    colorgen(&tmp, dateoutlinecolor, &dateoutline16);
    colorgen(&tmp, greeteroutlinecolor, &greeteroutline16);
    colorgen(&tmp, modifoutlinecolor, &modifoutline16);
}

static te_expr *compile_expression(const char *const from, const char *expression, const te_variable *variables, int var_count) {
    int te_err = 0;
    te_expr *expr = te_compile(expression, variables, var_count, &te_err);
    if (te_err) {
        fprintf(stderr, "Failed to reason about '%s' given by '%s'\n", expression, from);
        exit(EXIT_FAILURE);
    }
    return expr;
}

static DrawData create_draw_data() {
    DrawData draw_data;
    memset(&draw_data, 0, sizeof(DrawData));

    return draw_data;
}

static void draw_elements(cairo_t *const ctx, DrawData const *const draw_data) {
    // indicator stuff
    if (!bar_enabled) {
        draw_indic(ctx, draw_data->indicator_x, draw_data->indicator_y);
    } else {
        if (unlock_state == STATE_KEY_ACTIVE ||
            unlock_state == STATE_BACKSPACE_ACTIVE) {
            // note: might be biased to cause more hits on lower indices
            // maybe see about doing ((double) rand() / RAND_MAX) * bar_count
            int index = rand() % bar_count;
            bar_heights[index] = max_bar_height;
            for (int i = 0; i < ((max_bar_height / bar_step) + 1); ++i) {
                int low_ind = index - i;
                while (low_ind < 0) {
                    low_ind += bar_count;
                }
                int high_ind = (index + i) % bar_count;
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
        draw_bar(ctx, draw_data->bar_x, draw_data->bar_y, draw_data->bar_width, draw_data->screen_x, draw_data->screen_y);
    }

    draw_text(ctx, draw_data->status_text);
    draw_text(ctx, draw_data->keylayout_text);
    draw_text(ctx, draw_data->mod_text);
    draw_text(ctx, draw_data->time_text);
    draw_text(ctx, draw_data->date_text);
    draw_text(ctx, draw_data->greeter_text);
}

/*
 * Renders the lock screen on the provided drawable with the given resolution.
 */
void render_lock(uint32_t *resolution, xcb_drawable_t drawable) {
    const double scaling_factor = get_dpi_value() / 96.0;
    int button_diameter_physical = ceil(scaling_factor * BUTTON_DIAMETER);
    DEBUG("scaling_factor is %.f, physical diameter is %d px\n",
        scaling_factor, button_diameter_physical);

    if (!vistype)
        vistype = get_visualtype_by_depth(32, screen);
    /* Initialize cairo: Create one in-memory surface to render the unlock
     * indicator on, create one XCB surface to actually draw (one or more,
     * depending on the amount of screens) unlock indicators on.
     * create two more surfaces for time and date display
     */
    cairo_surface_t *output = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, resolution[0], resolution[1]);
    cairo_t *ctx = cairo_create(output);
    cairo_scale(ctx, scaling_factor, scaling_factor);

    //    cairo_set_font_face(ctx, get_font_face(0));

    cairo_surface_t *xcb_output = cairo_xcb_surface_create(conn, drawable, vistype, resolution[0], resolution[1]);
    cairo_t *xcb_ctx = cairo_create(xcb_output);

    /*update image according to the slideshow_interval*/
    if (slideshow_image_count > 0) {
        unsigned long now = (unsigned long)time(NULL);
        if (img == NULL || now - lastCheck >= slideshow_interval) {
            if (slideshow_random_selection) {
                img = img_slideshow[rand() % slideshow_image_count];
            } else {
                img = img_slideshow[current_slideshow_index++];

                if (current_slideshow_index >= slideshow_image_count) {
                    current_slideshow_index = 0;
                }
            }
            lastCheck = now;
        }
    }

    if (blur_bg_img) {
        cairo_set_source_surface(xcb_ctx, blur_bg_img, 0, 0);
        cairo_paint(xcb_ctx);
    } else {
        cairo_set_source_rgba(xcb_ctx, background.red, background.green, background.blue, background.alpha);
        cairo_rectangle(xcb_ctx, 0, 0, resolution[0], resolution[1]);
        cairo_fill(xcb_ctx);
    }

    if (img) {
        draw_image(resolution, img, xcb_ctx);
    }

    /*
     * gen text
     * calc vars
     * process if keystroke or not
     * draw indicator
     * draw text
     */
    DrawData draw_data = create_draw_data();

    if (unlock_indicator &&
        (unlock_state >= STATE_KEY_PRESSED || auth_state > STATE_AUTH_IDLE || show_indicator)) {
        switch (auth_state) {
            case STATE_AUTH_VERIFY:
                draw_data.status_text.show = true;
                strncpy(draw_data.status_text.str, verif_text, sizeof(draw_data.status_text.str) - 1);
                draw_data.status_text.font = get_font_face(VERIF_FONT);
                draw_data.status_text.color = verif16;
                draw_data.status_text.outline_color = verifoutline16;
                draw_data.status_text.size = verif_size;
                draw_data.status_text.outline_width = verifoutlinewidth;
                draw_data.status_text.align = verif_align;
                break;
            case STATE_AUTH_LOCK:
                draw_data.status_text.show = true;
                strncpy(draw_data.status_text.str, lock_text, sizeof(draw_data.status_text.str) - 1);
                draw_data.status_text.font = get_font_face(VERIF_FONT);
                draw_data.status_text.color = verif16;
                draw_data.status_text.outline_color = verifoutline16;
                draw_data.status_text.size = verif_size;
                draw_data.status_text.outline_width = verifoutlinewidth;
                draw_data.status_text.align = verif_align;
                break;
            case STATE_AUTH_WRONG:
                draw_data.status_text.show = true;
                strncpy(draw_data.status_text.str, wrong_text, sizeof(draw_data.status_text.str) - 1);
                draw_data.status_text.font = get_font_face(WRONG_FONT);
                draw_data.status_text.color = wrong16;
                draw_data.status_text.outline_color = wrongoutline16;
                draw_data.status_text.size = wrong_size;
                draw_data.status_text.outline_width = wrongoutlinewidth;
                draw_data.status_text.align = wrong_align;
                break;
            case STATE_I3LOCK_LOCK_FAILED:
                draw_data.status_text.show = true;
                strncpy(draw_data.status_text.str, lock_failed_text, sizeof(draw_data.status_text.str) - 1);
                draw_data.status_text.font = get_font_face(WRONG_FONT);
                draw_data.status_text.color = wrong16;
                draw_data.status_text.outline_color = wrongoutline16;
                draw_data.status_text.size = wrong_size;
                draw_data.status_text.outline_width = wrongoutlinewidth;
                draw_data.status_text.align = wrong_align;
                break;
            default:
                if (unlock_state == STATE_NOTHING_TO_DELETE) {
                    draw_data.status_text.show = true;
                    strncpy(draw_data.status_text.str, noinput_text, sizeof(draw_data.status_text.str) - 1);
                    draw_data.status_text.font = get_font_face(WRONG_FONT);
                    draw_data.status_text.color = wrong16;
                    draw_data.status_text.outline_color = wrongoutline16;
                    draw_data.status_text.size = wrong_size;
                    draw_data.status_text.outline_width = wrongoutlinewidth;
                    draw_data.status_text.align = wrong_align;
                    break;
                }
                if (show_failed_attempts && failed_attempts > 0) {
                    draw_data.status_text.show = true;
                    draw_data.status_text.font = get_font_face(WRONG_FONT);
                    draw_data.status_text.color = wrong16;
                    draw_data.status_text.outline_color = wrongoutline16;
                    draw_data.status_text.size = wrong_size;
                    draw_data.status_text.outline_width = wrongoutlinewidth;
                    draw_data.status_text.align = wrong_align;
                    // TODO: variable for this
                    draw_data.status_text.size = 32.0;
                    if (failed_attempts > 999) {
                        strncpy(draw_data.status_text.str, "> 999", sizeof(draw_data.status_text.str));
                    } else {
                        snprintf(draw_data.status_text.str, sizeof(draw_data.status_text.str), "%d", failed_attempts);
                    }
                }
                break;
        }
    }

    if (modifier_string) {
        draw_data.mod_text.show = true;
        strncpy(draw_data.mod_text.str, modifier_string, sizeof(draw_data.mod_text.str) - 1);
        draw_data.mod_text.size = modifier_size;
        draw_data.mod_text.outline_width = modifieroutlinewidth;
        draw_data.mod_text.font = get_font_face(WRONG_FONT);
        draw_data.mod_text.align = modif_align;
        draw_data.mod_text.color = modif16;
        draw_data.mod_text.outline_color = modifoutline16;
    }

    if (layout_text) {
        draw_data.keylayout_text.show = true;
        strncpy(draw_data.keylayout_text.str, layout_text, sizeof(draw_data.keylayout_text.str) - 1);
        draw_data.keylayout_text.size = layout_size;
        draw_data.keylayout_text.outline_width = layoutoutlinewidth;
        draw_data.keylayout_text.font = get_font_face(LAYOUT_FONT);
        draw_data.keylayout_text.color = layout16;
        draw_data.keylayout_text.outline_color = layoutoutline16;
        draw_data.keylayout_text.align = layout_align;
    }

    if (greeter_text) {
        draw_data.greeter_text.show = true;
        strncpy(draw_data.greeter_text.str, greeter_text, sizeof(draw_data.greeter_text.str) - 1);
        draw_data.greeter_text.size = greeter_size;
        draw_data.greeter_text.outline_width = greeteroutlinewidth;
        draw_data.greeter_text.font = get_font_face(GREETER_FONT);
        draw_data.greeter_text.color = greeter16;
        draw_data.greeter_text.outline_color = greeteroutline16;
        draw_data.greeter_text.align = greeter_align;
    }

    if (show_clock && (!draw_data.status_text.show || always_show_clock)) {
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(draw_data.time_text.str, 40, time_format, timeinfo);
        if (*draw_data.time_text.str) {
            draw_data.time_text.show = true;
            draw_data.time_text.size = time_size;
            draw_data.time_text.outline_width = timeoutlinewidth;
            draw_data.time_text.color = time16;
            draw_data.time_text.outline_color = timeoutline16;
            draw_data.time_text.font = get_font_face(TIME_FONT);
            draw_data.time_text.align = time_align;
        }
        strftime(draw_data.date_text.str, 40, date_format, timeinfo);
        if (*draw_data.date_text.str) {
            draw_data.date_text.show = true;
            draw_data.date_text.size = date_size;
            draw_data.date_text.outline_width = dateoutlinewidth;
            draw_data.date_text.color = date16;
            draw_data.date_text.outline_color = dateoutline16;
            draw_data.date_text.font = get_font_face(DATE_FONT);
            draw_data.date_text.align = date_align;
        }

        if (*draw_data.greeter_text.str) {
            draw_data.greeter_text.show = true;
            draw_data.greeter_text.size = greeter_size;
            draw_data.greeter_text.outline_width = greeteroutlinewidth;
            draw_data.greeter_text.color = greeter16;
            draw_data.greeter_text.outline_color = greeteroutline16;
            draw_data.greeter_text.font = get_font_face(GREETER_FONT);
            draw_data.greeter_text.align = greeter_align;
        }
    }

    // initialize positioning vars
    double screen_x = 0, screen_y = 0,
           width = 0, height = 0;

    double radius = (circle_radius + ring_width);
    DEBUG("scaling_factor is %f, physical diameter is %d px\n",
          scaling_factor, button_diameter_physical);

    // variable mapping for evaluating the clock position expression
    const unsigned int vars_size = 14;
    te_variable vars[] =
        {{"w", &width},
         {"h", &height},
         {"x", &screen_x},
         {"y", &screen_y},
         {"ix", &draw_data.indicator_x},
         {"iy", &draw_data.indicator_y},
         {"tx", &draw_data.time_text.x},
         {"ty", &draw_data.time_text.y},
         {"dx", &draw_data.date_text.x},
         {"dy", &draw_data.date_text.y},
         {"bw", &draw_data.bar_width},
         {"bx", &draw_data.bar_x},
         {"by", &draw_data.bar_y},
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
    te_expr *te_verif_x_expr = compile_expression("--verifpos", verif_x_expr, vars, vars_size);
    te_expr *te_verif_y_expr = compile_expression("--verifpos", verif_y_expr, vars, vars_size);
    te_expr *te_wrong_x_expr = compile_expression("--wrongpos", wrong_x_expr, vars, vars_size);
    te_expr *te_wrong_y_expr = compile_expression("--wrongpos", wrong_y_expr, vars, vars_size);
    te_expr *te_modif_x_expr = compile_expression("--modifpos", modif_x_expr, vars, vars_size);
    te_expr *te_modif_y_expr = compile_expression("--modifpos", modif_y_expr, vars, vars_size);
    te_expr *te_bar_x_expr = compile_expression("--bar-position", bar_x_expr, vars, vars_size);
    te_expr *te_bar_y_expr = strlen(bar_y_expr) ? compile_expression("--bar-position", bar_y_expr, vars, vars_size) : NULL;
    te_expr *te_bar_width_expr = strlen(bar_width_expr) ? compile_expression("--bar-width", bar_width_expr, vars, vars_size) : NULL;

    te_expr *te_greeter_x_expr = compile_expression("--greeterpos", greeter_x_expr, vars, vars_size);
    te_expr *te_greeter_y_expr = compile_expression("--greeterpos", greeter_y_expr, vars, vars_size);

    if (xr_screens > 0) {
        if (screen_number < 0 || screen_number > xr_screens) {
            screen_number = 0;
        }

        DEBUG("Drawing indicator on %d screens\n", screen_number);

        int current_screen = screen_number == 0 ? 0 : screen_number - 1;
        const int end_screen = screen_number == 0 ? xr_screens : screen_number;
        for (; current_screen < end_screen; current_screen++) {
            draw_data.indicator_x = 0;
            draw_data.indicator_y = 0;
            draw_data.time_text.x = 0;
            draw_data.time_text.y = 0;
            draw_data.date_text.x = 0;
            draw_data.date_text.y = 0;
            draw_data.greeter_text.x = 0;
            draw_data.greeter_text.y = 0;

            width = xr_resolutions[current_screen].width / scaling_factor;
            height = xr_resolutions[current_screen].height / scaling_factor;
            screen_x = xr_resolutions[current_screen].x / scaling_factor;
            screen_y = xr_resolutions[current_screen].y / scaling_factor;
            draw_data.screen_x = screen_x;
            draw_data.screen_y = screen_y;
            draw_data.indicator_x = te_eval(te_ind_x_expr);
            draw_data.indicator_y = te_eval(te_ind_y_expr);
            draw_data.time_text.x = te_eval(te_time_x_expr);
            draw_data.time_text.y = te_eval(te_time_y_expr);
            draw_data.date_text.x = te_eval(te_date_x_expr);
            draw_data.date_text.y = te_eval(te_date_y_expr);
            draw_data.keylayout_text.x = te_eval(te_layout_x_expr);
            draw_data.keylayout_text.y = te_eval(te_layout_y_expr);
            draw_data.greeter_text.x = te_eval(te_greeter_x_expr);
            draw_data.greeter_text.y = te_eval(te_greeter_y_expr);

            switch (auth_state) {
                case STATE_AUTH_VERIFY:
                case STATE_AUTH_LOCK:
                    draw_data.status_text.x = te_eval(te_verif_x_expr);
                    draw_data.status_text.y = te_eval(te_verif_y_expr);
                    break;
                case STATE_AUTH_WRONG:
                case STATE_I3LOCK_LOCK_FAILED:
                    draw_data.status_text.x = te_eval(te_wrong_x_expr);
                    draw_data.status_text.y = te_eval(te_wrong_y_expr);
                    break;
                default:
                    draw_data.status_text.x = te_eval(te_status_x_expr);
                    draw_data.status_text.y = te_eval(te_status_y_expr);
                    break;
            }

            draw_data.mod_text.x = te_eval(te_modif_x_expr);
            draw_data.mod_text.y = te_eval(te_modif_y_expr);

            if (te_bar_y_expr) {
                draw_data.bar_x = te_eval(te_bar_x_expr);
                draw_data.bar_y = te_eval(te_bar_y_expr);
            } else {
                double bar_offset = te_eval(te_bar_x_expr);
                if (bar_orientation == BAR_VERT) {
                    draw_data.bar_x = bar_offset;
                    draw_data.bar_y = screen_y;
                } else {
                    draw_data.bar_x = screen_x;
                    draw_data.bar_y = bar_offset;
                }
            }
            if (te_bar_width_expr)
                draw_data.bar_width = te_eval(te_bar_width_expr);
            else if (bar_orientation == BAR_VERT)
                draw_data.bar_width = height;
            else
                draw_data.bar_width = width;


            DEBUG("Indicator at %fx%f on screen %d\n", draw_data.indicator_x, draw_data.indicator_y, current_screen + 1);
            DEBUG("Bar at %fx%f with width %f on screen %d\n", draw_data.bar_x, draw_data.bar_y, draw_data.bar_width, current_screen + 1);
            DEBUG("Time at %fx%f on screen %d\n", draw_data.time_text.x, draw_data.time_text.y, current_screen + 1);
            DEBUG("Date at %fx%f on screen %d\n", draw_data.date_text.x, draw_data.date_text.y, current_screen + 1);
            DEBUG("Layout at %fx%f on screen %d\n", draw_data.keylayout_text.x, draw_data.keylayout_text.y, current_screen + 1);
            DEBUG("Status at %fx%f on screen %d\n", draw_data.status_text.x, draw_data.status_text.y, current_screen + 1);
            DEBUG("Mod at %fx%f on screen %d\n", draw_data.mod_text.x, draw_data.mod_text.y, current_screen + 1);
            // scale_draw_data(&draw_data, scaling_factor);
            draw_elements(ctx, &draw_data);
        }
    } else {
        /* We have no information about the screen sizes/positions, so we just
         * place the unlock indicator in the middle of the X root window and
         * hope for the best. */
        width = last_resolution[0] / scaling_factor;
        height = last_resolution[1] / scaling_factor;
        draw_data.screen_x = 0;
        draw_data.screen_y = 0;
        draw_data.indicator_x = width / 2;
        draw_data.indicator_y = height / 2;

        draw_data.time_text.x = te_eval(te_time_x_expr);
        draw_data.time_text.y = te_eval(te_time_y_expr);
        draw_data.date_text.x = te_eval(te_date_x_expr);
        draw_data.date_text.y = te_eval(te_date_y_expr);
        draw_data.keylayout_text.x = te_eval(te_layout_x_expr);
        draw_data.keylayout_text.y = te_eval(te_layout_y_expr);
        draw_data.greeter_text.x = te_eval(te_greeter_x_expr);
        draw_data.greeter_text.y = te_eval(te_greeter_y_expr);
        switch (auth_state) {
            case STATE_AUTH_VERIFY:
            case STATE_AUTH_LOCK:
                draw_data.status_text.x = te_eval(te_verif_x_expr);
                draw_data.status_text.y = te_eval(te_verif_y_expr);
                break;
            case STATE_AUTH_WRONG:
            case STATE_I3LOCK_LOCK_FAILED:
                draw_data.status_text.x = te_eval(te_wrong_x_expr);
                draw_data.status_text.y = te_eval(te_wrong_y_expr);
                break;
            default:
                draw_data.status_text.x = te_eval(te_status_x_expr);
                draw_data.status_text.y = te_eval(te_status_y_expr);
                break;
        }
        draw_data.mod_text.x = te_eval(te_modif_x_expr);
        draw_data.mod_text.y = te_eval(te_modif_y_expr);

        if (te_bar_y_expr) {
            draw_data.bar_x = te_eval(te_bar_x_expr);
            draw_data.bar_y = te_eval(te_bar_y_expr);
        } else {
            double bar_offset = te_eval(te_bar_x_expr);
            if (bar_orientation == BAR_VERT) {
                draw_data.bar_x = bar_offset;
                draw_data.bar_y = screen_y;
            } else {
                draw_data.bar_x = screen_x;
                draw_data.bar_y = bar_offset;
            }
        }
        if (te_bar_width_expr)
            draw_data.bar_width = te_eval(te_bar_width_expr);
        else if (bar_orientation == BAR_VERT)
            draw_data.bar_width = height;
        else
            draw_data.bar_width = width;

        DEBUG("Indicator at %fx%f\n", draw_data.indicator_x, draw_data.indicator_y);
        DEBUG("Bar at %fx%f with width %f\n", draw_data.bar_x, draw_data.bar_y, draw_data.bar_width);
        DEBUG("Time at %fx%f\n", draw_data.time_text.x, draw_data.time_text.y);
        DEBUG("Date at %fx%f\n", draw_data.date_text.x, draw_data.date_text.y);
        DEBUG("Layout at %fx%f\n", draw_data.keylayout_text.x, draw_data.keylayout_text.y);
        DEBUG("Status at %fx%f\n", draw_data.status_text.x, draw_data.status_text.y);
        DEBUG("Mod at %fx%f\n", draw_data.mod_text.x, draw_data.mod_text.y);

        draw_elements(ctx, &draw_data);
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
    te_free(te_verif_x_expr);
    te_free(te_verif_y_expr);
    te_free(te_wrong_x_expr);
    te_free(te_wrong_y_expr);
    te_free(te_modif_x_expr);
    te_free(te_modif_y_expr);
    te_free(te_bar_x_expr);
    te_free(te_bar_y_expr);
    te_free(te_bar_width_expr);
    te_free(te_greeter_x_expr);
    te_free(te_greeter_y_expr);

    cairo_set_source_surface(xcb_ctx, output, 0, 0);
    cairo_rectangle(xcb_ctx, 0, 0, resolution[0], resolution[1]);
    cairo_fill(xcb_ctx);

    cairo_surface_destroy(xcb_output);
    cairo_surface_destroy(output);
    cairo_destroy(ctx);
    cairo_destroy(xcb_ctx);
}

/**
 * Draws the configured image on the provided context. The image is drawn centered on all monitors, tiled, or just
 * painted starting from 0,0. It is also scaled if bg_type is FILL, MAX, or SCALE.
 */
void draw_image(uint32_t* root_resolution, cairo_surface_t *img, cairo_t* xcb_ctx) {

    if (bg_type == NONE) {
        // Don't do any image manipulation
        cairo_set_source_surface(xcb_ctx, img, 0, 0);
        cairo_paint(xcb_ctx);
        return;
    }

    cairo_pattern_t *pattern = cairo_pattern_create_for_surface(img);
    cairo_pattern_set_extend(pattern, bg_type == TILE ? CAIRO_EXTEND_REPEAT : CAIRO_EXTEND_NONE);
    cairo_set_source(xcb_ctx, pattern);

    double image_width = cairo_image_surface_get_width(img);
    double image_height = cairo_image_surface_get_height(img);

    for (int i = 0; i < xr_screens; i++) {
        // Find out scaling factors using bg_type and aspect ratios
        double scale_x = 1, scale_y = 1;
        if (bg_type == SCALE) {
            scale_x = xr_resolutions[i].width / image_width;
            scale_y = xr_resolutions[i].height / image_height;

        } else if (bg_type == MAX || bg_type == FILL) {
            double aspect_diff = (double) xr_resolutions[i].height / xr_resolutions[i].width - image_height / image_width;
            if((bg_type == MAX && aspect_diff >= 0) || (bg_type == FILL && aspect_diff <= 0)) {
                scale_x = scale_y = xr_resolutions[i].width / image_width;
            } else if ((bg_type == MAX && aspect_diff < 0) || (bg_type == FILL && aspect_diff > 0)) {
                scale_x = scale_y = xr_resolutions[i].height / image_height;
            }
        }

        // Scale and translate the pattern
        cairo_matrix_t matrix;
        cairo_matrix_init_scale(&matrix, 1/scale_x, 1/scale_y);

        if (bg_type == TILE) {
            // Start image from top-left corner
            cairo_matrix_translate(&matrix, -xr_resolutions[i].x, -xr_resolutions[i].y);
        } else {
            // Draw image in the center of the screen
            cairo_matrix_translate(&matrix,
                (image_width  * scale_x - xr_resolutions[i].width ) / 2 - xr_resolutions[i].x,
                (image_height * scale_y - xr_resolutions[i].height) / 2 - xr_resolutions[i].y);
        }

        cairo_pattern_set_matrix(pattern, &matrix);

        // Draw to screen
        cairo_rectangle(xcb_ctx, xr_resolutions[i].x, xr_resolutions[i].y, xr_resolutions[i].width, xr_resolutions[i].height);
        cairo_fill(xcb_ctx);
    }

    cairo_pattern_destroy(pattern);
}

/*
 * Calls render_lock on a new pixmap and swaps that with the current pixmap
 *
 */
void redraw_screen(void) {
    DEBUG("redraw_screen(unlock_state = %d, auth_state = %d) @ [%lu]\n", unlock_state, auth_state, (unsigned long)time(NULL));
    xcb_pixmap_t pixmap = create_bg_pixmap(conn, win, last_resolution, color);
    render_lock(last_resolution, pixmap);
    xcb_change_window_attributes(conn, win, XCB_CW_BACK_PIXMAP, (uint32_t[1]){pixmap});
    xcb_clear_area(conn, 0, win, 0, 0, last_resolution[0], last_resolution[1]);
    xcb_free_pixmap(conn, pixmap);
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
