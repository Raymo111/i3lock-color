/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2010 Michael Stapelberg
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
#include <ev.h>
#include <cairo.h>
#include <cairo/cairo-xcb.h>

#include "i3lock.h"
#include "xcb.h"
#include "unlock_indicator.h"
#include "xinerama.h"
#include "tinyexpr.h"

/* clock stuff */
#include <time.h>

extern double circle_radius;

#define BUTTON_RADIUS (circle_radius)
#define BUTTON_SPACE (BUTTON_RADIUS + 5)
#define BUTTON_CENTER (BUTTON_RADIUS + 5)
#define BUTTON_DIAMETER (2 * BUTTON_SPACE)
#define CLOCK_WIDTH 400
#define CLOCK_HEIGHT 200

/*******************************************************************************
 * Variables defined in i3lock.c.
 ******************************************************************************/

extern bool debug_mode;

/* The current position in the input buffer. Useful to determine if any
 * characters of the password have already been entered or not. */
int input_position;

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
extern char textcolor[9];
extern char timecolor[9];
extern char datecolor[9];
extern char keyhlcolor[9];
extern char bshlcolor[9];
extern char separatorcolor[9];
extern int internal_line_source;

extern int screen_number;
extern float refresh_rate;

extern bool show_clock;
extern bool show_indicator;
extern char time_format[32];
extern char date_format[32];
extern char time_font[32];
extern char date_font[32];
extern char time_x_expr[32];
extern char time_y_expr[32];
extern char date_x_expr[32];
extern char date_y_expr[32];

extern double time_size;
extern double date_size;
extern double text_size;
extern double modifier_size;

extern char* verif_text;
extern char* wrong_text;

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

/*
 * Returns the scaling factor of the current screen. E.g., on a 227 DPI MacBook
 * Pro 13" Retina screen, the scaling factor is 227/96 = 2.36.
 *
 */
static double scaling_factor(void) {
    const int dpi = (double)screen->height_in_pixels * 25.4 /
                    (double)screen->height_in_millimeters;
    return (dpi / 96.0);
}

/*
 * Draws global image with fill color onto a pixmap with the given
 * resolution and returns it.
 *
 */
xcb_pixmap_t draw_image(uint32_t *resolution) {
    xcb_pixmap_t bg_pixmap = XCB_NONE;
    int button_diameter_physical = ceil(scaling_factor() * BUTTON_DIAMETER);
    int clock_width_physical = ceil(scaling_factor() * CLOCK_WIDTH);
    int clock_height_physical = ceil(scaling_factor() * CLOCK_HEIGHT);
    DEBUG("scaling_factor is %.f, physical diameter is %d px\n",
          scaling_factor(), button_diameter_physical);

    if (!vistype)
        vistype = get_root_visual_type(screen);
    bg_pixmap = create_bg_pixmap(conn, screen, resolution, color);
    /* Initialize cairo: Create one in-memory surface to render the unlock
     * indicator on, create one XCB surface to actually draw (one or more,
     * depending on the amount of screens) unlock indicators on.
     * create two more surfaces for time and date display
     */
    cairo_surface_t *output = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, button_diameter_physical, button_diameter_physical);
    cairo_t *ctx = cairo_create(output);

    cairo_surface_t *time_output = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, clock_width_physical, clock_height_physical);
    cairo_t *time_ctx = cairo_create(time_output);

    cairo_surface_t *date_output = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, clock_width_physical, clock_height_physical);
    cairo_t *date_ctx = cairo_create(date_output);

    cairo_surface_t *xcb_output = cairo_xcb_surface_create(conn, bg_pixmap, vistype, resolution[0], resolution[1]);
    cairo_t *xcb_ctx = cairo_create(xcb_output);

    if (blur_img || img) {
        if (blur_img) {
            cairo_set_source_surface(xcb_ctx, blur_img, 0, 0);
            cairo_paint(xcb_ctx);
        }
        if (img) {
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
        char strgroups[3][3] = {{color[0], color[1], '\0'},
                                {color[2], color[3], '\0'},
                                {color[4], color[5], '\0'}};
        uint32_t rgb16[3] = {(strtol(strgroups[0], NULL, 16)),
                             (strtol(strgroups[1], NULL, 16)),
                             (strtol(strgroups[2], NULL, 16))};
        cairo_set_source_rgb(xcb_ctx, rgb16[0] / 255.0, rgb16[1] / 255.0, rgb16[2] / 255.0);
        cairo_rectangle(xcb_ctx, 0, 0, resolution[0], resolution[1]);
        cairo_fill(xcb_ctx);
    }

    /* build indicator color arrays */
    char strgroupsiv[4][3] = {{insidevercolor[0], insidevercolor[1], '\0'},
                              {insidevercolor[2], insidevercolor[3], '\0'},
                              {insidevercolor[4], insidevercolor[5], '\0'},
                              {insidevercolor[6], insidevercolor[7], '\0'}};
    uint32_t insidever16[4] = {(strtol(strgroupsiv[0], NULL, 16)),
                               (strtol(strgroupsiv[1], NULL, 16)),
                               (strtol(strgroupsiv[2], NULL, 16)),
                               (strtol(strgroupsiv[3], NULL, 16))};
    char strgroupsiw[4][3] = {{insidewrongcolor[0], insidewrongcolor[1], '\0'},
                              {insidewrongcolor[2], insidewrongcolor[3], '\0'},
                              {insidewrongcolor[4], insidewrongcolor[5], '\0'},
                              {insidewrongcolor[6], insidewrongcolor[7], '\0'}};
    uint32_t insidewrong16[4] = {(strtol(strgroupsiw[0], NULL, 16)),
                                 (strtol(strgroupsiw[1], NULL, 16)),
                                 (strtol(strgroupsiw[2], NULL, 16)),
                                 (strtol(strgroupsiw[3], NULL, 16))};
    char strgroupsi[4][3] = {{insidecolor[0], insidecolor[1], '\0'},
                             {insidecolor[2], insidecolor[3], '\0'},
                             {insidecolor[4], insidecolor[5], '\0'},
                             {insidecolor[6], insidecolor[7], '\0'}};
    uint32_t inside16[4] = {(strtol(strgroupsi[0], NULL, 16)),
                            (strtol(strgroupsi[1], NULL, 16)),
                            (strtol(strgroupsi[2], NULL, 16)),
                            (strtol(strgroupsi[3], NULL, 16))};
    char strgroupsrv[4][3] = {{ringvercolor[0], ringvercolor[1], '\0'},
                              {ringvercolor[2], ringvercolor[3], '\0'},
                              {ringvercolor[4], ringvercolor[5], '\0'},
                              {ringvercolor[6], ringvercolor[7], '\0'}};
    uint32_t ringver16[4] = {(strtol(strgroupsrv[0], NULL, 16)),
                             (strtol(strgroupsrv[1], NULL, 16)),
                             (strtol(strgroupsrv[2], NULL, 16)),
                             (strtol(strgroupsrv[3], NULL, 16))};
    char strgroupsrw[4][3] = {{ringwrongcolor[0], ringwrongcolor[1], '\0'},
                              {ringwrongcolor[2], ringwrongcolor[3], '\0'},
                              {ringwrongcolor[4], ringwrongcolor[5], '\0'},
                              {ringwrongcolor[6], ringwrongcolor[7], '\0'}};
    uint32_t ringwrong16[4] = {(strtol(strgroupsrw[0], NULL, 16)),
                               (strtol(strgroupsrw[1], NULL, 16)),
                               (strtol(strgroupsrw[2], NULL, 16)),
                               (strtol(strgroupsrw[3], NULL, 16))};
    char strgroupsr[4][3] = {{ringcolor[0], ringcolor[1], '\0'},
                             {ringcolor[2], ringcolor[3], '\0'},
                             {ringcolor[4], ringcolor[5], '\0'},
                             {ringcolor[6], ringcolor[7], '\0'}};
    uint32_t ring16[4] = {(strtol(strgroupsr[0], NULL, 16)),
                          (strtol(strgroupsr[1], NULL, 16)),
                          (strtol(strgroupsr[2], NULL, 16)),
                          (strtol(strgroupsr[3], NULL, 16))};
    char strgroupsl[4][3] = {{linecolor[0], linecolor[1], '\0'},
                             {linecolor[2], linecolor[3], '\0'},
                             {linecolor[4], linecolor[5], '\0'},
                             {linecolor[6], linecolor[7], '\0'}};
    uint32_t line16[4] = {(strtol(strgroupsl[0], NULL, 16)),
                          (strtol(strgroupsl[1], NULL, 16)),
                          (strtol(strgroupsl[2], NULL, 16)),
                          (strtol(strgroupsl[3], NULL, 16))};
    char strgroupst[4][3] = {{textcolor[0], textcolor[1], '\0'},
                             {textcolor[2], textcolor[3], '\0'},
                             {textcolor[4], textcolor[5], '\0'},
                             {textcolor[6], textcolor[7], '\0'}};
    uint32_t text16[4] = {(strtol(strgroupst[0], NULL, 16)),
                          (strtol(strgroupst[1], NULL, 16)),
                          (strtol(strgroupst[2], NULL, 16)),
                          (strtol(strgroupst[3], NULL, 16))};
    char strgroupsc[4][3] = {{timecolor[0], timecolor[1], '\0'},
                             {timecolor[2], timecolor[3], '\0'},
                             {timecolor[4], timecolor[5], '\0'},
                             {timecolor[6], timecolor[7], '\0'}};
    uint32_t time16[4] = {(strtol(strgroupsc[0], NULL, 16)),
                          (strtol(strgroupsc[1], NULL, 16)),
                          (strtol(strgroupsc[2], NULL, 16)),
                          (strtol(strgroupsc[3], NULL, 16))};
    char strgroupsd[4][3] = {{datecolor[0], datecolor[1], '\0'},
                             {datecolor[2], datecolor[3], '\0'},
                             {datecolor[4], datecolor[5], '\0'},
                             {datecolor[6], datecolor[7], '\0'}};
    uint32_t date16[4] = {(strtol(strgroupsd[0], NULL, 16)),
                          (strtol(strgroupsd[1], NULL, 16)),
                          (strtol(strgroupsd[2], NULL, 16)),
                          (strtol(strgroupsd[3], NULL, 16))};
    char strgroupsk[4][3] = {{keyhlcolor[0], keyhlcolor[1], '\0'},
                             {keyhlcolor[2], keyhlcolor[3], '\0'},
                             {keyhlcolor[4], keyhlcolor[5], '\0'},
                             {keyhlcolor[6], keyhlcolor[7], '\0'}};
    uint32_t keyhl16[4] = {(strtol(strgroupsk[0], NULL, 16)),
                           (strtol(strgroupsk[1], NULL, 16)),
                           (strtol(strgroupsk[2], NULL, 16)),
                           (strtol(strgroupsk[3], NULL, 16))};
    char strgroupsb[4][3] = {{bshlcolor[0], bshlcolor[1], '\0'},
                             {bshlcolor[2], bshlcolor[3], '\0'},
                             {bshlcolor[4], bshlcolor[5], '\0'},
                             {bshlcolor[6], bshlcolor[7], '\0'}};
    uint32_t bshl16[4] = {(strtol(strgroupsb[0], NULL, 16)),
                          (strtol(strgroupsb[1], NULL, 16)),
                          (strtol(strgroupsb[2], NULL, 16)),
                          (strtol(strgroupsb[3], NULL, 16))};
    char strgroupss[4][3] = {{separatorcolor[0], separatorcolor[1], '\0'},
                             {separatorcolor[2], separatorcolor[3], '\0'},
                             {separatorcolor[4], separatorcolor[5], '\0'},
                             {separatorcolor[6], separatorcolor[7], '\0'}};
    uint32_t sep16[4] = {(strtol(strgroupss[0], NULL, 16)),
                          (strtol(strgroupss[1], NULL, 16)),
                          (strtol(strgroupss[2], NULL, 16)),
                          (strtol(strgroupss[3], NULL, 16))};

    /* https://github.com/ravinrabbid/i3lock-clock/commit/0de3a411fa5249c3a4822612c2d6c476389a1297 */
    time_t rawtime;
    struct tm* timeinfo;
    bool unlock_indic_text = false;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (unlock_indicator &&
        (unlock_state >= STATE_KEY_PRESSED || auth_state > STATE_AUTH_IDLE || show_indicator)) {
        cairo_scale(ctx, scaling_factor(), scaling_factor());
        /* Draw a (centered) circle with transparent background. */
        cairo_set_line_width(ctx, 7.0);
        cairo_arc(ctx,
                  BUTTON_CENTER /* x */,
                  BUTTON_CENTER /* y */,
                  BUTTON_RADIUS /* radius */,
                  0 /* start */,
                  2 * M_PI /* end */);

        /* Use the appropriate color for the different PAM states
         * (currently verifying, wrong password, or default) */
        switch (auth_state) {
            case STATE_AUTH_VERIFY:
            case STATE_AUTH_LOCK:
                cairo_set_source_rgba(ctx, (double)insidever16[0]/255, (double)insidever16[1]/255, (double)insidever16[2]/255, (double)insidever16[3]/255);
                break;
            case STATE_AUTH_WRONG:
            case STATE_I3LOCK_LOCK_FAILED:
                cairo_set_source_rgba(ctx, (double)insidewrong16[0]/255, (double)insidewrong16[1]/255, (double)insidewrong16[2]/255, (double)insidewrong16[3]/255);
                break;
            default:
                cairo_set_source_rgba(ctx, (double)inside16[0]/255, (double)inside16[1]/255, (double)inside16[2]/255, (double)inside16[3]/255);
                break;
        }
        cairo_fill_preserve(ctx);

        switch (auth_state) {
            case STATE_AUTH_VERIFY:
            case STATE_AUTH_LOCK:
                cairo_set_source_rgba(ctx, (double)ringver16[0]/255, (double)ringver16[1]/255, (double)ringver16[2]/255, (double)ringver16[3]/255);
                if (internal_line_source == 1) {
                  line16[0] = ringver16[0];
                  line16[1] = ringver16[1];
                  line16[2] = ringver16[2];
                  line16[3] = ringver16[3];
                }
                break;
            case STATE_AUTH_WRONG:
            case STATE_I3LOCK_LOCK_FAILED:
                cairo_set_source_rgba(ctx, (double)ringwrong16[0]/255, (double)ringwrong16[1]/255, (double)ringwrong16[2]/255, (double)ringwrong16[3]/255);
                if (internal_line_source == 1) {
                  line16[0] = ringwrong16[0];
                  line16[1] = ringwrong16[1];
                  line16[2] = ringwrong16[2];
                  line16[3] = ringwrong16[3];
                }
                break;
            case STATE_AUTH_IDLE:
                cairo_set_source_rgba(ctx, (double)ring16[0]/255, (double)ring16[1]/255, (double)ring16[2]/255, (double)ring16[3]/255);
                if (internal_line_source == 1) {
                  line16[0] = ring16[0];
                  line16[1] = ring16[1];
                  line16[2] = ring16[2];
                  line16[3] = ring16[3];
                }
                break;
        }
        cairo_stroke(ctx);

        /* Draw an inner separator line. */
        if (internal_line_source != 2) { //pretty sure this only needs drawn if it's being drawn over the inside?
          cairo_set_source_rgba(ctx, (double)line16[0]/255, (double)line16[1]/255, (double)line16[2]/255, (double)line16[3]/255);
          cairo_set_line_width(ctx, 2.0);
          cairo_arc(ctx,
                    BUTTON_CENTER /* x */,
                    BUTTON_CENTER /* y */,
                    BUTTON_RADIUS - 5 /* radius */,
                    0,
                    2 * M_PI);
          cairo_stroke(ctx);
        }

        cairo_set_line_width(ctx, 10.0);

        char *text = NULL;

        /* We don't want to show more than a 3-digit number. */
        char buf[4];

        cairo_set_source_rgba(ctx, (double)text16[0]/255, (double)text16[1]/255, (double)text16[2]/255, (double)text16[3]/255);
        cairo_select_font_face(ctx, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(ctx, text_size);
        switch (auth_state) {
            case STATE_AUTH_VERIFY:
                text = verif_text;
                break;
            case STATE_AUTH_LOCK:
                text = "locking…";
                break;
            case STATE_AUTH_WRONG:
                text = wrong_text;
                break;
            case STATE_I3LOCK_LOCK_FAILED:
                text = "lock failed!";
                break;
            default:
                if (show_failed_attempts && failed_attempts > 0) {
                    if (failed_attempts > 999) {
                        text = "> 999";
                    } else {
                        snprintf(buf, sizeof(buf), "%d", failed_attempts);
                        text = buf;
                    }
                    cairo_set_font_size(ctx, 32.0);
                }
                break;
        }

        if (text) {
            unlock_indic_text = true;
            cairo_text_extents_t extents;
            double x, y;

            cairo_text_extents(ctx, text, &extents);
            x = BUTTON_CENTER - ((extents.width / 2) + extents.x_bearing);
            y = BUTTON_CENTER - ((extents.height / 2) + extents.y_bearing);

            cairo_move_to(ctx, x, y);
            cairo_show_text(ctx, text);
            cairo_close_path(ctx);
        }

        if (auth_state == STATE_AUTH_WRONG && (modifier_string != NULL)) {
            cairo_text_extents_t extents;
            double x, y;

            cairo_set_font_size(ctx, modifier_size);

            cairo_text_extents(ctx, modifier_string, &extents);
            x = BUTTON_CENTER - ((extents.width / 2) + extents.x_bearing);
            y = BUTTON_CENTER - ((extents.height / 2) + extents.y_bearing) + 28.0;

            cairo_move_to(ctx, x, y);
            cairo_show_text(ctx, modifier_string);
            cairo_close_path(ctx);
        }

        /* After the user pressed any valid key or the backspace key, we
         * highlight a random part of the unlock indicator to confirm this
         * keypress. */
        if (unlock_state == STATE_KEY_ACTIVE ||
            unlock_state == STATE_BACKSPACE_ACTIVE) {
            cairo_set_line_width(ctx, 7.0);
            cairo_new_sub_path(ctx);
            double highlight_start = (rand() % (int)(2 * M_PI * 100)) / 100.0;
            cairo_arc(ctx,
                      BUTTON_CENTER /* x */,
                      BUTTON_CENTER /* y */,
                      BUTTON_RADIUS /* radius */,
                      highlight_start,
                      highlight_start + (M_PI / 3.0));
            if (unlock_state == STATE_KEY_ACTIVE) {
                /* For normal keys, we use a lighter green. */ //lol no
                cairo_set_source_rgba(ctx, (double)keyhl16[0]/255, (double)keyhl16[1]/255, (double)keyhl16[2]/255, (double)keyhl16[3]/255);
            } else {
                /* For backspace, we use red. */ //lol no
                cairo_set_source_rgba(ctx, (double)bshl16[0]/255, (double)bshl16[1]/255, (double)bshl16[2]/255, (double)bshl16[3]/255);
            }

            cairo_stroke(ctx);

            /* Draw two little separators for the highlighted part of the
             * unlock indicator. */
            cairo_set_source_rgba(ctx, (double)sep16[0]/255, (double)sep16[1]/255, (double)sep16[2]/255, (double)sep16[3]/255);
            cairo_arc(ctx,
                      BUTTON_CENTER /* x */,
                      BUTTON_CENTER /* y */,
                      BUTTON_RADIUS /* radius */,
                      highlight_start /* start */,
                      highlight_start + (M_PI / 128.0) /* end */);
            cairo_stroke(ctx);
            cairo_arc(ctx,
                      BUTTON_CENTER /* x */,
                      BUTTON_CENTER /* y */,
                      BUTTON_RADIUS /* radius */,
                      (highlight_start + (M_PI / 3.0)) - (M_PI / 128.0) /* start */,
                      highlight_start + (M_PI / 3.0) /* end */);
            cairo_stroke(ctx);
        }
    }

    if (show_clock && !unlock_indic_text) {
        char *text = NULL;
        char *date = NULL;
        char time_text[40] = {0};
        char date_text[40] = {0};

        strftime(time_text, 40, time_format, timeinfo);
        strftime(date_text, 40, date_format, timeinfo);
        text = time_text;
        date = date_text;

        if (text) {
            double x, y;
            cairo_text_extents_t extents;

            cairo_set_font_size(time_ctx, time_size);
            cairo_select_font_face(time_ctx, time_font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_source_rgba(time_ctx, (double)time16[0]/255, (double)time16[1]/255, (double)time16[2]/255, (double)time16[3]/255);

            cairo_text_extents(time_ctx, text, &extents);
            x = CLOCK_WIDTH/2 - ((extents.width / 2) + extents.x_bearing);
            y = CLOCK_HEIGHT/2;

            cairo_move_to(time_ctx, x, y);
            cairo_show_text(time_ctx, text);
            cairo_close_path(time_ctx);
        }

        if (date) {
            double x, y;
            cairo_text_extents_t extents;

            cairo_select_font_face(date_ctx, date_font, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
            cairo_set_source_rgba(date_ctx, (double)date16[0]/255, (double)date16[1]/255, (double)date16[2]/255, (double)date16[3]/255);
            cairo_set_font_size(date_ctx, date_size);

            cairo_text_extents(date_ctx, date, &extents);
            x = CLOCK_WIDTH/2 - ((extents.width / 2) + extents.x_bearing);
            y = CLOCK_HEIGHT/2;

            cairo_move_to(date_ctx, x, y);
            cairo_show_text(date_ctx, date);
            cairo_close_path(date_ctx);
        }
    }

    double ix, iy;
    double x, y;
    double screen_x, screen_y;
    double w, h;
    double tx = 0;
    double ty = 0;

    double clock_width = CLOCK_WIDTH;
    double clock_height = CLOCK_HEIGHT;

    int te_x_err;
    int te_y_err;
    // variable mapping for evaluating the clock position expression
    te_variable vars[] = {
        {"w", &w}, {"h", &h},
        {"x", &screen_x}, {"y", &screen_y},
        {"ix", &ix}, {"iy", &iy},
        {"tx", &tx}, {"ty", &ty},
        {"cw", &clock_width}, {"ch", &clock_height} // pretty sure this is fine.
    };

    te_expr *te_time_x_expr = te_compile(time_x_expr, vars, 10, &te_x_err);
    te_expr *te_time_y_expr = te_compile(time_y_expr, vars, 10, &te_y_err);
    te_expr *te_date_x_expr = te_compile(date_x_expr, vars, 10, &te_x_err);
    te_expr *te_date_y_expr = te_compile(date_y_expr, vars, 10, &te_y_err);

    if (xr_screens > 0) {
        /* Composite the unlock indicator in the middle of each screen. */
        // excuse me, just gonna hack something in right here
        if (screen_number != -1 && screen_number < xr_screens) {
            w = xr_resolutions[screen_number].width;
            h = xr_resolutions[screen_number].height;
            screen_x = xr_resolutions[screen_number].x;
            screen_y = xr_resolutions[screen_number].y;
            ix = xr_resolutions[screen_number].x + (xr_resolutions[screen_number].width / 2);
            iy = xr_resolutions[screen_number].y + (xr_resolutions[screen_number].height / 2);
            x = ix - (button_diameter_physical / 2);
            y = iy - (button_diameter_physical / 2);
            cairo_set_source_surface(xcb_ctx, output, x, y);
            cairo_rectangle(xcb_ctx, x, y, button_diameter_physical, button_diameter_physical);
            cairo_fill(xcb_ctx);

            if (te_time_x_expr && te_time_y_expr) {
                tx = 0;
                ty = 0;
                tx = te_eval(te_time_x_expr);
                ty = te_eval(te_time_y_expr);
                double time_x = tx;
                double time_y = ty;
                double date_x = te_eval(te_date_x_expr);
                double date_y = te_eval(te_date_y_expr);
                DEBUG("tx: %f ty: %f ix: %f, iy: %f\n", tx, ty, ix, iy);
                DEBUG("\ttime_x: %f time_y: %f date_x: %f date_y: %f screen_number: %d\n", time_x, time_y, date_x, date_y, screen_number);
                DEBUG("\tscreen x: %d screen y: %d screen w: %f screen h: %f\n", xr_resolutions[screen_number].x, xr_resolutions[screen_number].y, w, h);
                cairo_set_source_surface(xcb_ctx, time_output, time_x, time_y);
                cairo_rectangle(xcb_ctx, time_x, time_y, CLOCK_WIDTH, CLOCK_HEIGHT);
                cairo_fill(xcb_ctx);
                cairo_set_source_surface(xcb_ctx, date_output, date_x, date_y);
                cairo_rectangle(xcb_ctx, date_x, date_y, CLOCK_WIDTH, CLOCK_HEIGHT);
                cairo_fill(xcb_ctx);
            }
        } else {
            for (int screen = 0; screen < xr_screens; screen++) {
                w = xr_resolutions[screen].width;
                h = xr_resolutions[screen].height;
                screen_x = xr_resolutions[screen].x;
                screen_y = xr_resolutions[screen].y;
                ix = xr_resolutions[screen].x + (xr_resolutions[screen].width / 2);
                iy = xr_resolutions[screen].y + (xr_resolutions[screen].height / 2);
                x = ix - (button_diameter_physical / 2);
                y = iy - (button_diameter_physical / 2);
                cairo_set_source_surface(xcb_ctx, output, x, y);
                cairo_rectangle(xcb_ctx, x, y, button_diameter_physical, button_diameter_physical);
                cairo_fill(xcb_ctx);
                if (te_time_x_expr && te_time_y_expr) {
                    tx = 0;
                    ty = 0;
                    tx = te_eval(te_time_x_expr);
                    ty = te_eval(te_time_y_expr);
                    double time_x = tx;
                    double time_y = ty;
                    double date_x = te_eval(te_date_x_expr);
                    double date_y = te_eval(te_date_y_expr);
                    DEBUG("tx: %f ty: %f f ix: %f iy: %f\n", tx, ty, ix, iy);
                    DEBUG("\ttime_x: %f time_y: %f date_x: %f date_y: %f screen_number: %d\n", time_x, time_y, date_x, date_y, screen);
                    DEBUG("\tscreen x: %d screen y: %d screen w: %f screen h: %f\n", xr_resolutions[screen].x, xr_resolutions[screen].y, w, h);
                    cairo_set_source_surface(xcb_ctx, time_output, time_x, time_y);
                    cairo_rectangle(xcb_ctx, time_x, time_y, CLOCK_WIDTH, CLOCK_HEIGHT);
                    cairo_fill(xcb_ctx);
                    cairo_set_source_surface(xcb_ctx, date_output, date_x, date_y);
                    cairo_rectangle(xcb_ctx, date_x, date_y, CLOCK_WIDTH, CLOCK_HEIGHT);
                    cairo_fill(xcb_ctx);
                } else {
                    DEBUG("error codes for exprs are %d, %d\n", te_x_err, te_y_err);
                    DEBUG("exprs: %s, %s\n", time_x_expr, time_y_expr);
                }
            }
        }
    } else {
        /* We have no information about the screen sizes/positions, so we just
         * place the unlock indicator in the middle of the X root window and
         * hope for the best. */
        w = last_resolution[0];
        h = last_resolution[1];
        ix = last_resolution[0] / 2;
        iy = last_resolution[1] / 2;
        x = ix - (button_diameter_physical / 2);
        y = iy - (button_diameter_physical / 2);
        cairo_set_source_surface(xcb_ctx, output, x, y);
        cairo_rectangle(xcb_ctx, x, y, button_diameter_physical, button_diameter_physical);
        cairo_fill(xcb_ctx);
        if (te_time_x_expr && te_time_y_expr) {
            tx = te_eval(te_time_x_expr);
            ty = te_eval(te_time_y_expr);
            double time_x = tx - CLOCK_WIDTH / 2;
            double time_y = tx - CLOCK_HEIGHT / 2;
            double date_x = te_eval(te_date_x_expr) - CLOCK_WIDTH / 2;
            double date_y = te_eval(te_date_y_expr) - CLOCK_HEIGHT / 2;
            DEBUG("Placing time at %f, %f\n", time_x, time_y);
            cairo_set_source_surface(xcb_ctx, time_output, time_x, time_y);
            cairo_rectangle(xcb_ctx, time_x, time_y, CLOCK_WIDTH, CLOCK_HEIGHT);
            cairo_fill(xcb_ctx);
            cairo_set_source_surface(xcb_ctx, date_output, date_x, date_y);
            cairo_rectangle(xcb_ctx, date_x, date_y, CLOCK_WIDTH, CLOCK_HEIGHT);
            cairo_fill(xcb_ctx);
        }
    }

    cairo_surface_destroy(xcb_output);
    cairo_surface_destroy(time_output);
    cairo_surface_destroy(date_output);
    cairo_surface_destroy(output);
    cairo_destroy(ctx);
    cairo_destroy(time_ctx);
    cairo_destroy(date_ctx);
    cairo_destroy(xcb_ctx);
    return bg_pixmap;
}

/*
 * Calls draw_image on a new pixmap and swaps that with the current pixmap
 *
 */
void redraw_screen(void) {
    DEBUG("redraw_screen(unlock_state = %d, auth_state = %d)\n", unlock_state, auth_state);
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

static void time_redraw_cb(struct ev_loop *loop, ev_periodic *w, int revents) {
    redraw_screen();
}

void start_time_redraw_tick(struct ev_loop* main_loop) {
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
