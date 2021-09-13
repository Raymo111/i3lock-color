#ifndef _UNLOCK_INDICATOR_H
#define _UNLOCK_INDICATOR_H

#include <ev.h>
#include <xcb/xcb.h>

#include <fonts.h>

typedef enum {
    STATE_STARTED = 0,           /* default state */
    STATE_KEY_PRESSED = 1,       /* key was pressed, show unlock indicator */
    STATE_KEY_ACTIVE = 2,        /* a key was pressed recently, highlight part
                                   of the unlock indicator. */
    STATE_BACKSPACE_ACTIVE = 3,  /* backspace was pressed recently, highlight
                                   part of the unlock indicator in red. */
    STATE_NOTHING_TO_DELETE = 4, /* backspace was pressed, but there is nothing to delete. */
} unlock_state_t;

typedef enum {
    STATE_AUTH_IDLE = 0,          /* no authenticator interaction at the moment */
    STATE_AUTH_VERIFY = 1,        /* currently verifying the password via authenticator */
    STATE_AUTH_LOCK = 2,          /* currently locking the screen */
    STATE_AUTH_WRONG = 3,         /* the password was wrong */
    STATE_I3LOCK_LOCK_FAILED = 4, /* i3lock failed to load */
} auth_state_t;

typedef struct {
    text_t status_text;
    text_t mod_text;
    text_t keylayout_text;
    text_t time_text;
    text_t date_text;
    text_t greeter_text;

    double indicator_x, indicator_y;

    double screen_x, screen_y;
    double bar_x, bar_y, bar_width;
} DrawData;

typedef enum {
    NONE,
    TILE,
    CENTER,
    FILL,
    SCALE,
    MAX,
} background_type_t;

void render_lock(uint32_t* resolution, xcb_drawable_t drawable);
void draw_image(uint32_t* resolution, cairo_surface_t* img, cairo_t* xcb_ctx);
void init_colors_once(void);
void redraw_screen(void);
void clear_indicator(void);
void start_time_redraw_timeout(void);
void* start_time_redraw_tick_pthread(void* arg);
void start_time_redraw_tick(struct ev_loop* main_loop);
#endif
