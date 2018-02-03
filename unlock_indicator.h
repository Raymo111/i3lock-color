#ifndef UNLOCK_INDICATOR_H
#define UNLOCK_INDICATOR_H

#include <ev.h>
#include <xcb/xcb.h>

typedef struct rgb {
	double red;
	double green;
	double blue;
} rgb_t;

typedef struct rgb_str {
	char red[3];
	char green[3];
	char blue[3];
} rgb_str_t;

typedef struct rgba {
	double red;
	double green;
	double blue;
	double alpha;
} rgba_t;

typedef struct rgba_str {
	char red[3];
	char green[3];
	char blue[3];
	char alpha[3];
} rgba_str_t;

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

xcb_pixmap_t draw_image(uint32_t* resolution);
void init_colors_once(void);
void redraw_screen(void);
void clear_indicator(void);
void start_time_redraw_timeout(void);
void* start_time_redraw_tick_pthread(void* arg);
void start_time_redraw_tick(struct ev_loop* main_loop);
#endif
