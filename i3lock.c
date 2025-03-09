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
#include <config.h>

#include <pthread.h>
#include <math.h>

#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <xcb/xproto.h>
#include <err.h>
#include <errno.h>
#include <assert.h>
#include <regex.h>
#ifdef __OpenBSD__
#include <bsd_auth.h>
#else
#include <security/pam_appl.h>
#endif
#include <getopt.h>
#include <string.h>
#include <ev.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>
#if XKBCOMPOSE == 1
#include <xkbcommon/xkbcommon-compose.h>
#endif
#include <xkbcommon/xkbcommon-x11.h>
#include <cairo.h>
#include <cairo/cairo-xcb.h>
#ifdef HAVE_EXPLICIT_BZERO
#include <strings.h> /* explicit_bzero(3) */
#endif
#include <xcb/xcb_aux.h>
#include <xcb/randr.h>

#include "i3lock.h"
#include "xcb.h"
#include "cursors.h"
#include "unlock_indicator.h"
#include "randr.h"
#include "dpi.h"
#include "blur.h"
#include "jpg.h"
#include "fonts.h"

#include <gif_lib.h>

#define TSTAMP_N_SECS(n) (n * 1.0)
#define TSTAMP_N_MINS(n) (60 * TSTAMP_N_SECS(n))
#define START_TIMER(timer_obj, timeout, callback) \
    timer_obj = start_timer(timer_obj, timeout, callback)
#define STOP_TIMER(timer_obj) \
    timer_obj = stop_timer(timer_obj)

typedef void (*ev_callback_t)(EV_P_ ev_timer *w, int revents);
static void input_done(void);

char color[9] = "a3a3a3ff";

/* options for unlock indicator colors */
char insidevercolor[9] = "006effbf";
char insidewrongcolor[9] = "fa0000bf";
char insidecolor[9] = "000000bf";
char ringvercolor[9] = "3300faff";
char ringwrongcolor[9] = "7d3300ff";
char ringcolor[9] = "337d00ff";
char linecolor[9] = "000000ff";
char verifcolor[9] = "000000ff";
char wrongcolor[9] = "000000ff";
char layoutcolor[9] = "000000ff";
char timecolor[9] = "000000ff";
char datecolor[9] = "000000ff";
char modifcolor[9] = "000000ff";
char keyhlcolor[9] = "33db00ff";
char bshlcolor[9] = "db3300ff";
char separatorcolor[9] = "000000ff";
char greetercolor[9] = "000000ff";

char verifoutlinecolor[9] = "00000000";
char wrongoutlinecolor[9] = "00000000";
char layoutoutlinecolor[9] = "00000000";
char timeoutlinecolor[9] = "00000000";
char dateoutlinecolor[9] = "00000000";
char greeteroutlinecolor[9] = "00000000";
char modifoutlinecolor[9] = "00000000";

/* int defining which display the lock indicator should be shown on. If -1, then show on all displays.*/
int screen_number = 0;

/* default is to use the supplied line color, 1 will be ring color, 2 will be to use the inside color for ver/wrong/etc */
int internal_line_source = 0;

/* refresh rate in seconds, default to 1s */
float refresh_rate = 1.0;

bool show_clock = false;
bool slideshow_enabled = false;
bool always_show_clock = false;
bool show_indicator = false;
bool show_modkey_text = true;

/* there's some issues with compositing - upstream removed support for this, but we'll allow people to supply an arg to enable it */
bool composite = false;
/* time formatter strings for date/time
    I picked 32-length char arrays because some people might want really funky time formatters.
    Who am I to judge?
*/
/*
 * 0 = center
 * 1 = left
 * 2 = right
 */
int verif_align = 0;
int wrong_align = 0;
int time_align = 0;
int date_align = 0;
int layout_align = 0;
int modif_align = 0;
int greeter_align = 0;

char time_format[32] = "%H:%M:%S\0";
char date_format[32] = "%A, %m %Y\0";

char verif_font[64] = "sans-serif\0";
char wrong_font[64] = "sans-serif\0";
char layout_font[64] = "sans-serif\0";
char time_font[64] = "sans-serif\0";
char date_font[64] = "sans-serif\0";
char greeter_font[64] = "sans-serif\0";

char* fonts[6] = {
    verif_font,
    wrong_font,
    layout_font,
    time_font,
    date_font,
    greeter_font
};

char ind_x_expr[32] = "x + (w / 2)\0";
char ind_y_expr[32] = "y + (h / 2)\0";
char time_x_expr[32] = "ix\0";
char time_y_expr[32] = "iy\0";
char date_x_expr[32] = "tx\0";
char date_y_expr[32] = "ty+30\0";
char layout_x_expr[32] = "dx\0";
char layout_y_expr[32] = "dy+30\0";
char status_x_expr[32] = "ix\0";
char status_y_expr[32] = "iy\0";
char modif_x_expr[32] = "ix\0";
char modif_y_expr[32] = "iy+28\0";
char verif_x_expr[32] = "ix\0";
char verif_y_expr[32] = "iy\0";
char wrong_x_expr[32] = "ix\0";
char wrong_y_expr[32] = "iy\0";
char greeter_x_expr[32] = "ix\0";
char greeter_y_expr[32] = "iy\0";

double time_size = 32.0;
double date_size = 14.0;
double verif_size = 28.0;
double wrong_size = 28.0;
double modifier_size = 14.0;
double layout_size = 14.0;
double circle_radius = 90.0;
double ring_width = 7.0;
double greeter_size = 32.0;

double timeoutlinewidth = 0;
double dateoutlinewidth = 0;
double verifoutlinewidth = 0;
double wrongoutlinewidth = 0;
double modifieroutlinewidth = 0;
double layoutoutlinewidth = 0;
double greeteroutlinewidth = 0;

char* verif_text = "verifying…";
char* wrong_text = "wrong!";
char* noinput_text = "no input";
char* lock_text = "locking…";
char* lock_failed_text = "lock failed!";
int   keylayout_mode = -1;
char* layout_text = NULL;
char* greeter_text = "";

/* opts for blurring */
bool blur = false;
bool step_blur = false;
int blur_sigma = 5;

/* do not verify password */
bool no_verify = false;

uint32_t last_resolution[2];
xcb_window_t win;
static xcb_cursor_t cursor;
#ifndef __OpenBSD__
static pam_handle_t *pam_handle;
static bool pam_cleanup;
#endif
int input_position = 0;
/* Holds the password you enter (in UTF-8). */
static char password[512];
static bool beep = false;
bool debug_mode = false;
bool unlock_indicator = true;
char *modifier_string = NULL;
static bool dont_fork = false;
struct ev_loop *main_loop;
static struct ev_timer *clear_auth_wrong_timeout;
static struct ev_timer *clear_indicator_timeout;
static struct ev_timer *discard_passwd_timeout;
extern unlock_state_t unlock_state;
extern auth_state_t auth_state;
int failed_attempts = 0;
bool show_failed_attempts = false;
bool retry_verification = false;

static struct xkb_state *xkb_state;
static struct xkb_context *xkb_context;
static struct xkb_keymap *xkb_keymap;
#if XKBCOMPOSE == 1
static struct xkb_compose_table *xkb_compose_table;
static struct xkb_compose_state *xkb_compose_state;
#endif
static uint8_t xkb_base_event;
static uint8_t xkb_base_error;
static int randr_base = -1;

char *image_path = NULL;
char *image_raw_format = NULL;
char *slideshow_path = NULL;

struct gif {
    cairo_surface_t *img;
    float delay_sec;
};

struct gif *gif_img = NULL;
int gif_img_count = 0;

cairo_surface_t *img = NULL;
char *img_slideshow[256];
cairo_surface_t *blur_bg_img = NULL;
int slideshow_image_count = 0;
int slideshow_interval = 10;
bool slideshow_random_selection = false;

background_type_t bg_type = NONE;

bool ignore_empty_password = false;
bool skip_repeated_empty_password = false;
bool pass_media_keys = false;
bool pass_screen_keys = false;
bool pass_power_keys = false;
bool pass_volume_keys = false;

bool hotkeys = false;
char* cmd_brightness_up = NULL;
char* cmd_brightness_down = NULL;

char* cmd_media_play = NULL;
char* cmd_media_pause = NULL;
char* cmd_media_stop = NULL;
char* cmd_media_next = NULL;
char* cmd_media_prev = NULL;

char* cmd_audio_mute = NULL;
char* cmd_volume_up = NULL;
char* cmd_volume_down = NULL;
char* cmd_mic_mute = NULL;

char* cmd_power_down = NULL;
char* cmd_power_off = NULL;
char* cmd_power_sleep = NULL;

// for the rendering thread, so we can clean it up
pthread_t draw_thread;
// main thread still sometimes calls redraw()
// allow you to disable. handy if you use bar with lots of crap.
bool redraw_thread = false;

// experimental bar stuff
#define BAR_VERT 0
#define BAR_FLAT 1
#define BAR_DEFAULT 0
#define BAR_REVERSED 1
#define BAR_BIDIRECTIONAL 2
#define MAX_BAR_COUNT 65535
#define MIN_BAR_COUNT 1

bool bar_enabled = false;
double *bar_heights = NULL;
double bar_step = 15;
double bar_base_height = 25;
double bar_periodic_step = 15;
double max_bar_height = 25;
int bar_count = 10;
int bar_orientation = BAR_FLAT;

char bar_base_color[9] = "000000ff";
char bar_x_expr[32] = "0";
char bar_y_expr[32] = ""; // empty string on y means use x as offset based on orientation
char bar_width_expr[32] = ""; // empty string means full width based on bar orientation
bool bar_bidirectional = false;
bool bar_reversed = false;

enum IMAGE_FORMAT {
    IMAGE_FORMAT_UNKNOWN,
    IMAGE_FORMAT_RAW,
    IMAGE_FORMAT_PNG,
    IMAGE_FORMAT_JPG,
    IMAGE_FORMAT_GIF
};

/* isutf, u8_dec © 2005 Jeff Bezanson, public domain */
#define isutf(c) (((c)&0xC0) != 0x80)

/*
 * Checks if the given path leads to an actual file or something else, e.g. a directory
 */
int is_directory(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

/*
 * Decrements i to point to the previous unicode glyph
 *
 */
static void u8_dec(char *s, int *i) {
    (void)(isutf(s[--(*i)]) || isutf(s[--(*i)]) || isutf(s[--(*i)]) || --(*i));
}

/*
 * fetches the keylayout name
 *      -1 (do not)
 * arg: 0 (show full string returned)
 *      1 (show the text, sans parenthesis)
 *      2 (show just what's in the parenthesis)
 *
 * credit to the XKB/xcb implementation (no libx11) from https://gist.github.com/bluetech/6061368
 * docs are really sparse, so finding some random implementation was nice
 */
static char* get_keylayoutname(int mode, xcb_connection_t* conn) {
    if (mode < 0 || mode > 2) return NULL;
    char *newans = NULL, *newans2 = NULL, *answer = xcb_get_key_group_names(conn);
    int substringStart = 0, substringEnd = 0, size = 0;
    DEBUG("keylayout answer is: [%s]\n", answer);
    switch (mode) {
        case 1:
            // truncate the string at the first parens
            for (int i = 0; answer[i] != '\0'; ++i) {
                if (answer[i] == '(') {
                    if (i != 0 && answer[i - 1] == ' ') {
                        answer[i - 1] = '\0';
                        break;
                    } else {
                        answer[i] = '\0';
                        break;
                    }
                }
            }
            break;
        case 2:
            for (int i = 0; answer[i] != '\0'; ++i) {
                if (answer[i] == '(') {
                    newans = &answer[i + 1];
                    substringStart = i + 1;
                } else if (answer[i] == ')' && newans != NULL) {
                    answer[i] = '\0';
                    substringEnd = i;
                    break;
                }
            }
            if (newans != NULL) {
                size = sizeof(char) * (substringEnd - substringStart + 1);
                newans2 = malloc(size);
                memcpy(newans2, newans, size);
                free(answer);
                answer = newans2;
            }
            break;
        case 0:
            // fall through
        default:
            break;
    }
    DEBUG("answer after mode parsing: [%s]\n", answer);
    // Free symbolic names structures
    return answer;
}

/*
 * Loads the XKB keymap from the X11 server and feeds it to xkbcommon.
 * Necessary so that we can properly let xkbcommon track the keyboard state and
 * translate keypresses to utf-8.
 *
 */

static bool load_keymap(void) {
    if (xkb_context == NULL) {
        if ((xkb_context = xkb_context_new(0)) == NULL) {
            fprintf(stderr, "[i3lock] could not create xkbcommon context\n");
            return false;
        }
    }

    xkb_keymap_unref(xkb_keymap);

    int32_t device_id = xkb_x11_get_core_keyboard_device_id(conn);
    DEBUG("device = %d\n", device_id);
    if ((xkb_keymap = xkb_x11_keymap_new_from_device(xkb_context, conn, device_id, 0)) == NULL) {
        fprintf(stderr, "[i3lock] xkb_x11_keymap_new_from_device failed\n");
        return false;
    }

    struct xkb_state *new_state =
        xkb_x11_state_new_from_device(xkb_keymap, conn, device_id);
    if (new_state == NULL) {
        fprintf(stderr, "[i3lock] xkb_x11_state_new_from_device failed\n");
        return false;
    }

    xkb_state_unref(xkb_state);
    xkb_state = new_state;

    return true;
}

#if XKBCOMPOSE == 1
/*
 * Loads the XKB compose table from the given locale.
 *
 */
static bool load_compose_table(const char *locale) {
    xkb_compose_table_unref(xkb_compose_table);

    if ((xkb_compose_table = xkb_compose_table_new_from_locale(xkb_context, locale, 0)) == NULL) {
        fprintf(stderr, "[i3lock] xkb_compose_table_new_from_locale failed\n");
        return false;
    }

    struct xkb_compose_state *new_compose_state = xkb_compose_state_new(xkb_compose_table, 0);
    if (new_compose_state == NULL) {
        fprintf(stderr, "[i3lock] xkb_compose_state_new failed\n");
        return false;
    }

    xkb_compose_state_unref(xkb_compose_state);
    xkb_compose_state = new_compose_state;

    return true;
}
#endif /* XKBCOMPOSE */

/*
 * Clears the memory which stored the password to be a bit safer against
 * cold-boot attacks.
 *
 */
static void clear_password_memory(void) {
#ifdef HAVE_EXPLICIT_BZERO
    /* Use explicit_bzero(3) which was explicitly designed not to be
     * optimized out by the compiler. */
    explicit_bzero(password, strlen(password));
#else
    /* A volatile pointer to the password buffer to prevent the compiler from
     * optimizing this out. */
    volatile char *vpassword = password;
    for (size_t c = 0; c < sizeof(password); c++)
        /* We store a non-random pattern which consists of the (irrelevant)
         * index plus (!) the value of the beep variable. This prevents the
         * compiler from optimizing the calls away, since the value of 'beep'
         * is not known at compile-time. */
        vpassword[c] = c + (int)beep;
#endif
}

ev_timer *start_timer(ev_timer *timer_obj, ev_tstamp timeout, ev_callback_t callback) {
    if (timer_obj) {
        ev_timer_stop(main_loop, timer_obj);
        ev_timer_set(timer_obj, timeout, 0.);
        ev_timer_start(main_loop, timer_obj);
    } else {
        /* When there is no memory, we just don’t have a timeout. We cannot
         * exit() here, since that would effectively unlock the screen. */
        timer_obj = calloc(sizeof(struct ev_timer), 1);
        if (timer_obj) {
            ev_timer_init(timer_obj, callback, timeout, 0.);
            ev_timer_start(main_loop, timer_obj);
        }
    }
    return timer_obj;
}

ev_timer *stop_timer(ev_timer *timer_obj) {
    if (timer_obj) {
        ev_timer_stop(main_loop, timer_obj);
        free(timer_obj);
    }
    return NULL;
}

/*
 * Neccessary calls after ending input via enter or others
 *
 */
static void finish_input(void) {
    password[input_position] = '\0';
    unlock_state = STATE_KEY_PRESSED;
    redraw_screen();
    input_done();
}

/*
 * Resets auth_state to STATE_AUTH_IDLE 2 seconds after an unsuccessful
 * authentication event.
 *
 */
static void clear_auth_wrong(EV_P_ ev_timer *w, int revents) {
    DEBUG("clearing auth wrong\n");
    auth_state = STATE_AUTH_IDLE;
    redraw_screen();

    /* Clear modifier string. */
    if (modifier_string != NULL) {
        free(modifier_string);
        modifier_string = NULL;
    }

    /* Now free this timeout. */
    STOP_TIMER(clear_auth_wrong_timeout);

    /* retry with input done during auth verification */
    if (retry_verification) {
        retry_verification = false;
        finish_input();
    }
}

static void clear_indicator_cb(EV_P_ ev_timer *w, int revents) {
    clear_indicator();
    STOP_TIMER(clear_indicator_timeout);
}

static void clear_input(void) {
    input_position = 0;
    clear_password_memory();
    password[input_position] = '\0';
}

static void discard_passwd_cb(EV_P_ ev_timer *w, int revents) {
    clear_input();
    STOP_TIMER(discard_passwd_timeout);
}

static void input_done(void) {
    STOP_TIMER(clear_auth_wrong_timeout);
    auth_state = STATE_AUTH_VERIFY;
    unlock_state = STATE_STARTED;
    redraw_screen();

    if (no_verify) {
        ev_break(EV_DEFAULT, EVBREAK_ALL);
        return;
    }

#ifdef __OpenBSD__
    struct passwd *pw;

    if (!(pw = getpwuid(getuid())))
        errx(1, "unknown uid %u.", getuid());

    if (auth_userokay(pw->pw_name, NULL, NULL, password) != 0) {
        DEBUG("successfully authenticated\n");
        clear_password_memory();

        ev_break(EV_DEFAULT, EVBREAK_ALL);
        return;
    }
#else
    if (pam_authenticate(pam_handle, 0) == PAM_SUCCESS) {
        DEBUG("successfully authenticated\n");
        clear_password_memory();

        /* PAM credentials should be refreshed, this will for example update any kerberos tickets.
         * Related to credentials pam_end() needs to be called to cleanup any temporary
         * credentials like kerberos /tmp/krb5cc_pam_* files which may of been left behind if the
         * refresh of the credentials failed. */
        pam_setcred(pam_handle, PAM_REFRESH_CRED);
        pam_cleanup = true;

        ev_break(EV_DEFAULT, EVBREAK_ALL);
        return;
    }
#endif

    if (debug_mode)
        fprintf(stderr, "Authentication failure\n");

    /* Get state of Caps and Num lock modifiers, to be displayed in
     * STATE_AUTH_WRONG state */
    xkb_mod_index_t idx, num_mods;
    const char *mod_name;

    num_mods = xkb_keymap_num_mods(xkb_keymap);

    for (idx = 0; idx < num_mods; idx++) {
        if (!xkb_state_mod_index_is_active(xkb_state, idx, XKB_STATE_MODS_EFFECTIVE))
            continue;

        mod_name = xkb_keymap_mod_get_name(xkb_keymap, idx);
        if (mod_name == NULL)
            continue;

        /* Replace certain xkb names with nicer, human-readable ones. */
        if (strcmp(mod_name, XKB_MOD_NAME_CAPS) == 0)
            mod_name = "Caps Lock";
        else if (strcmp(mod_name, XKB_MOD_NAME_ALT) == 0)
            mod_name = "Alt";
        else if (strcmp(mod_name, XKB_MOD_NAME_NUM) == 0)
            mod_name = "Num Lock";
        else if (strcmp(mod_name, XKB_MOD_NAME_LOGO) == 0)
            mod_name = "Super";

        if (show_modkey_text) {
            char *tmp;
            if (modifier_string == NULL) {
                if (asprintf(&tmp, "%s", mod_name) != -1)
                    modifier_string = tmp;
            } else if (asprintf(&tmp, "%s, %s", modifier_string, mod_name) != -1) {
                free(modifier_string);
                modifier_string = tmp;
            }
        }
    }

    auth_state = STATE_AUTH_WRONG;
    failed_attempts += 1;
    clear_input();
    if (unlock_indicator)
        redraw_screen();

    /* Clear this state after 2 seconds (unless the user enters another
     * password during that time). */
    ev_now_update(main_loop);
    START_TIMER(clear_auth_wrong_timeout, TSTAMP_N_SECS(2), clear_auth_wrong);

    /* Cancel the clear_indicator_timeout, it would hide the unlock indicator
     * too early. */
    STOP_TIMER(clear_indicator_timeout);

    /* beep on authentication failure, if enabled */
    if (beep) {
        xcb_bell(conn, 100);
        xcb_flush(conn);
    }
}

static void redraw_timeout(EV_P_ ev_timer *w, int revents) {
    redraw_screen();
    STOP_TIMER(w);
}

static bool skip_without_validation(void) {
    if (input_position != 0)
        return false;

    if (skip_repeated_empty_password || ignore_empty_password)
        return true;

    return false;
}

/*
 * Handle key presses. Fixes state, then looks up the key symbol for the
 * given keycode, then looks up the key symbol (as UCS-2), converts it to
 * UTF-8 and stores it in the password array.
 *
 */
static void handle_key_press(xcb_key_press_event_t *event) {
    xkb_keysym_t ksym;
    char buffer[128];
    int n;
    bool ctrl;
#if XKBCOMPOSE == 1
    bool composed = false;
#endif

    ksym = xkb_state_key_get_one_sym(xkb_state, event->detail);
    ctrl = xkb_state_mod_name_is_active(xkb_state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_DEPRESSED);

    /* The buffer will be null-terminated, so n >= 2 for 1 actual character. */
    memset(buffer, '\0', sizeof(buffer));

#if XKBCOMPOSE == 1
    if (xkb_compose_state && xkb_compose_state_feed(xkb_compose_state, ksym) == XKB_COMPOSE_FEED_ACCEPTED) {
        switch (xkb_compose_state_get_status(xkb_compose_state)) {
            case XKB_COMPOSE_NOTHING:
                break;
            case XKB_COMPOSE_COMPOSING:
                return;
            case XKB_COMPOSE_COMPOSED:
                /* xkb_compose_state_get_utf8 doesn't include the terminating byte in the return value
             * as xkb_keysym_to_utf8 does. Adding one makes the variable n consistent. */
                n = xkb_compose_state_get_utf8(xkb_compose_state, buffer, sizeof(buffer)) + 1;
                ksym = xkb_compose_state_get_one_sym(xkb_compose_state);
                composed = true;
                break;
            case XKB_COMPOSE_CANCELLED:
                xkb_compose_state_reset(xkb_compose_state);
                return;
        }
    }

    if (!composed) {
        n = xkb_keysym_to_utf8(ksym, buffer, sizeof(buffer));
    }
#else
    n = xkb_keysym_to_utf8(ksym, buffer, sizeof(buffer));
#endif

    //custom key commands
    if (hotkeys) {
        switch(ksym) {
            case XKB_KEY_XF86MonBrightnessUp:
                if (cmd_brightness_up) {
                    system(cmd_brightness_up);
                    return;
                }
                break;
            case XKB_KEY_XF86MonBrightnessDown:
                if (cmd_brightness_down) {
                    system(cmd_brightness_down);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioPlay:
                if (cmd_media_play) {
                    system(cmd_media_play);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioPause:
                if (cmd_media_pause) {
                    system(cmd_media_pause);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioStop:
                if (cmd_media_stop) {
                    system(cmd_media_stop);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioPrev:
                if (cmd_media_prev) {
                    system(cmd_media_prev);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioNext:
                if (cmd_media_next) {
                    system(cmd_media_next);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioMute:
                if (cmd_audio_mute) {
                    system(cmd_audio_mute);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioLowerVolume:
                if (cmd_volume_down) {
                    system(cmd_volume_down);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioRaiseVolume:
                if (cmd_volume_up) {
                    system(cmd_volume_up);
                    return;
                }
                break;
            case XKB_KEY_XF86AudioMicMute:
                if (cmd_mic_mute) {
                    system(cmd_mic_mute);
                    return;
                }
                break;
            case XKB_KEY_XF86PowerDown:
                if (cmd_power_down) {
                    system(cmd_power_down);
                    return;
                }
                break;
            case XKB_KEY_XF86PowerOff:
                if (cmd_power_off) {
                    system(cmd_power_off);
                    return;
                }
                break;
            case XKB_KEY_XF86Sleep:
                if (cmd_power_sleep) {
                    system(cmd_power_sleep);
                    return;
                }
                break;
        }
    }

    // media keys
    if (pass_media_keys) {
        switch(ksym) {
            case XKB_KEY_XF86AudioPlay:
            case XKB_KEY_XF86AudioPause:
            case XKB_KEY_XF86AudioStop:
            case XKB_KEY_XF86AudioPrev:
            case XKB_KEY_XF86AudioNext:
            case XKB_KEY_XF86AudioMute:
            case XKB_KEY_XF86AudioLowerVolume:
            case XKB_KEY_XF86AudioRaiseVolume:
            case XKB_KEY_XF86AudioMicMute:
                xcb_send_event(conn, true, screen->root, XCB_EVENT_MASK_BUTTON_PRESS, (char *)event);
                return;
        }
    }

	// screen keys
    if (pass_screen_keys) {
        switch(ksym) {
            case XKB_KEY_XF86MonBrightnessUp:
            case XKB_KEY_XF86MonBrightnessDown:
                xcb_send_event(conn, true, screen->root, XCB_EVENT_MASK_BUTTON_PRESS, (char *)event);
                return;
        }
    }

	// power keys
    if (pass_power_keys) {
        switch(ksym) {
            case XKB_KEY_XF86PowerDown:
            case XKB_KEY_XF86PowerOff:
            case XKB_KEY_XF86Sleep:
                xcb_send_event(conn, true, screen->root, XCB_EVENT_MASK_BUTTON_PRESS, (char *)event);
                return;
        }
    }

    // volume keys
    if (pass_volume_keys) {
        switch(ksym) {
            case XKB_KEY_XF86AudioMute:
            case XKB_KEY_XF86AudioLowerVolume:
            case XKB_KEY_XF86AudioRaiseVolume:
                xcb_send_event(conn, true, screen->root, XCB_EVENT_MASK_BUTTON_PRESS, (char *)event);
                return;
        }
    }

    // return/enter/etc
    switch (ksym) {
        case XKB_KEY_j:
        case XKB_KEY_m:
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
        case XKB_KEY_XF86ScreenSaver:
            if ((ksym == XKB_KEY_j || ksym == XKB_KEY_m) && !ctrl)
                break;

            if (auth_state == STATE_AUTH_WRONG) {
                retry_verification = true;
                return;
            }

            if (skip_without_validation()) {
                clear_input();
                return;
            }
            finish_input();
            skip_repeated_empty_password = true;
            return;
        default:
            skip_repeated_empty_password = false;
            // A new password is being entered, but a previous one is pending.
            // Discard the old one and clear the retry_verification flag.
            if (retry_verification) {
                retry_verification = false;
                clear_input();
            }
    }

    // backspace, esc, delete, etc
    switch (ksym) {
        case XKB_KEY_u:
        case XKB_KEY_Escape:
            if ((ksym == XKB_KEY_u && ctrl) ||
                ksym == XKB_KEY_Escape) {
                DEBUG("C-u pressed\n");
                clear_input();
                /* Also hide the unlock indicator */
                if (unlock_indicator)
                    clear_indicator();
                return;
            }
            break;

        case XKB_KEY_Delete:
        case XKB_KEY_KP_Delete:
            /* Deleting forward doesn’t make sense, as i3lock doesn’t allow you
             * to move the cursor when entering a password. We need to eat this
             * key press so that it won’t be treated as part of the password,
             * see issue #50. */
            return;

        case XKB_KEY_h:
        case XKB_KEY_BackSpace:
            if (ksym == XKB_KEY_h && !ctrl)
                break;

            if (input_position == 0) {
                START_TIMER(clear_indicator_timeout, 1.0, clear_indicator_cb);
                unlock_state = STATE_NOTHING_TO_DELETE;
                redraw_screen();
                return;
            }

            /* decrement input_position to point to the previous glyph */
            u8_dec(password, &input_position);
            password[input_position] = '\0';

            /* Hide the unlock indicator after a bit if the password buffer is
             * empty. */
            START_TIMER(clear_indicator_timeout, 1.0, clear_indicator_cb);
            unlock_state = STATE_BACKSPACE_ACTIVE;
            redraw_screen();
            unlock_state = STATE_KEY_PRESSED;
            return;
    }

    if ((input_position + 8) >= (int)sizeof(password))
        return;

#if 0
    /* FIXME: handle all of these? */
    printf("is_keypad_key = %d\n", xcb_is_keypad_key(sym));
    printf("is_private_keypad_key = %d\n", xcb_is_private_keypad_key(sym));
    printf("xcb_is_cursor_key = %d\n", xcb_is_cursor_key(sym));
    printf("xcb_is_pf_key = %d\n", xcb_is_pf_key(sym));
    printf("xcb_is_function_key = %d\n", xcb_is_function_key(sym));
    printf("xcb_is_misc_function_key = %d\n", xcb_is_misc_function_key(sym));
    printf("xcb_is_modifier_key = %d\n", xcb_is_modifier_key(sym));
#endif

    if (n < 2)
        return;

    /* store it in the password array as UTF-8 */
    memcpy(password + input_position, buffer, n - 1);
    input_position += n - 1;
    DEBUG("current password = %.*s\n", input_position, password);

    if (unlock_indicator) {
        unlock_state = STATE_KEY_ACTIVE;
        redraw_screen();
        unlock_state = STATE_KEY_PRESSED;

        struct ev_timer *timeout = NULL;
        START_TIMER(timeout, TSTAMP_N_SECS(0.25), redraw_timeout);
        STOP_TIMER(clear_indicator_timeout);
    }

    START_TIMER(discard_passwd_timeout, TSTAMP_N_MINS(3), discard_passwd_cb);
}

/*
 * A visibility notify event will be received when the visibility (= can the
 * user view the complete window) changes, so for example when a popup overlays
 * some area of the i3lock window.
 *
 * In this case, we raise our window on top so that the popup (or whatever is
 * hiding us) gets hidden.
 *
 */
static void handle_visibility_notify(xcb_connection_t *conn,
                                     xcb_visibility_notify_event_t *event) {
    if (event->state != XCB_VISIBILITY_UNOBSCURED) {
        uint32_t values[] = {XCB_STACK_MODE_ABOVE};
        xcb_configure_window(conn, event->window, XCB_CONFIG_WINDOW_STACK_MODE, values);
        xcb_flush(conn);
    }
}

/*
 * Called when the keyboard mapping changes. We update our symbols.
 *
 * We ignore errors — if the new keymap cannot be loaded it’s better if the
 * screen stays locked and the user intervenes by using killall i3lock.
 *
 */
static void process_xkb_event(xcb_generic_event_t *gevent) {
    union xkb_event {
        struct {
            uint8_t response_type;
            uint8_t xkbType;
            uint16_t sequence;
            xcb_timestamp_t time;
            uint8_t deviceID;
        } any;
        xcb_xkb_new_keyboard_notify_event_t new_keyboard_notify;
        xcb_xkb_map_notify_event_t map_notify;
        xcb_xkb_state_notify_event_t state_notify;
    } *event = (union xkb_event *)gevent;

    DEBUG("process_xkb_event for device %d\n", event->any.deviceID);

    if (event->any.deviceID != xkb_x11_get_core_keyboard_device_id(conn))
        return;

    /*
     * XkbNewKkdNotify and XkbMapNotify together capture all sorts of keymap
     * updates (e.g. xmodmap, xkbcomp, setxkbmap), with minimal redundent
     * recompilations.
     */
    switch (event->any.xkbType) {
        case XCB_XKB_NEW_KEYBOARD_NOTIFY:
            if (event->new_keyboard_notify.changed & XCB_XKB_NKN_DETAIL_KEYCODES)
                (void)load_keymap();
            break;

        case XCB_XKB_MAP_NOTIFY:
            (void)load_keymap();
            break;

        case XCB_XKB_STATE_NOTIFY:
            xkb_state_update_mask(xkb_state,
                                  event->state_notify.baseMods,
                                  event->state_notify.latchedMods,
                                  event->state_notify.lockedMods,
                                  event->state_notify.baseGroup,
                                  event->state_notify.latchedGroup,
                                  event->state_notify.lockedGroup);
  			if (layout_text != NULL) {
                  free(layout_text);
                  layout_text = NULL;
            }
            layout_text = get_keylayoutname(keylayout_mode, conn);
            redraw_screen();
            break;
    }
}

/*
 * Called when the properties on the root window change, e.g. when the screen
 * resolution changes. If so we update the window to cover the whole screen
 * and also redraw the image, if any.
 *
 */
static void handle_screen_resize(void) {
    xcb_get_geometry_cookie_t geomc;
    xcb_get_geometry_reply_t *geom;
    geomc = xcb_get_geometry(conn, screen->root);
    if ((geom = xcb_get_geometry_reply(conn, geomc, 0)) == NULL)
        return;

    if (last_resolution[0] == geom->width &&
        last_resolution[1] == geom->height) {
        free(geom);
        return;
    }

    last_resolution[0] = geom->width;
    last_resolution[1] = geom->height;

    free(geom);

    redraw_screen();

    uint32_t mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    xcb_configure_window(conn, win, mask, last_resolution);
    xcb_flush(conn);

    randr_query(screen->root);
    redraw_screen();
}

static ssize_t read_raw_image_native(uint32_t *dest, FILE *src, size_t width, size_t height, int pixstride) {
    ssize_t count = 0;
    for (size_t y = 0; y < height; y++) {
        size_t n = fread(&dest[y * pixstride], 1, width * 4, src);
        count += n;
        if (n < (size_t)(width * 4))
            break;
    }

    return count;
}

struct raw_pixel_format {
    int bpp;
    int red;
    int green;
    int blue;
};

static ssize_t read_raw_image_fmt(uint32_t *dest, FILE *src, size_t width, size_t height, int pixstride,
                                  struct raw_pixel_format fmt) {
    unsigned char *buf = malloc(width * fmt.bpp);
    if (buf == NULL)
        return -1;

    ssize_t count = 0;
    for (size_t y = 0; y < height; y++) {
        size_t n = fread(buf, 1, width * fmt.bpp, src);
        count += n;
        if (n < (size_t)(width * fmt.bpp))
            break;

        for (size_t x = 0; x < width; ++x) {
            int idx = x * fmt.bpp;
            dest[y * pixstride + x] = 0 |
                                      (buf[idx + fmt.red]) << 16 |
                                      (buf[idx + fmt.green]) << 8 |
                                      (buf[idx + fmt.blue]);
        }
    }

    free(buf);
    return count;
}

// Pre-defind pixel formats (<bytes per pixel>, <red pixel>, <green pixel>, <blue pixel>)
static const struct raw_pixel_format raw_fmt_rgb = {3, 0, 1, 2};
static const struct raw_pixel_format raw_fmt_rgbx = {4, 0, 1, 2};
static const struct raw_pixel_format raw_fmt_xrgb = {4, 1, 2, 3};
static const struct raw_pixel_format raw_fmt_bgr = {3, 2, 1, 0};
static const struct raw_pixel_format raw_fmt_bgrx = {4, 2, 1, 0};
static const struct raw_pixel_format raw_fmt_xbgr = {4, 3, 2, 1};

static cairo_surface_t *read_gif_image(const char *image_path) {
    int err;
    int width, stride, height;
    int bg_idx;
    uint32_t bg_color;
    ColorMapObject *cmap;
    GraphicsControlBlock gc;

    /* Open and load a GIF file */
    GifFileType *gif = DGifOpenFileName(image_path, &err);
    if (!gif) {
        fprintf(stderr, "Could not open GIF file, (Error %d)\n", err);
        return NULL;
    }
    if (DGifSlurp(gif) != GIF_OK) {
        fprintf(stderr, "Could not read the GIF image, (Error %d)\n", gif->Error);
        goto read_gif_image_clean;
    }

    /* Load canvas properties */
    width = gif->SWidth;
    height = gif->SHeight;
    cmap = gif->SColorMap;
    bg_idx = gif->SBackGroundColor;
    GifColorType *cmap_bg_rgb = cmap->Colors + bg_idx;
    bg_color = cmap_bg_rgb->Blue | cmap_bg_rgb->Green << 8 | cmap_bg_rgb->Red << 16;
    stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, width) / sizeof(uint32_t);
    if (stride < 0) {
        fprintf(stderr, "Invalid distance between beginning of rows\n");
        goto read_gif_image_clean;
    }

    /* Read the images into the RGB buffers */
    gif_img = malloc(gif->ImageCount * sizeof(struct gif));
    gif_img_count = gif->ImageCount;
    if (!gif_img) {
        fprintf(stderr, "Could not allocate memory for GIF image buffers\n");
        goto read_gif_image_clean;
    }
    uint32_t *data_prev = NULL;
    for (SavedImage *pimg = gif->SavedImages; pimg < gif->SavedImages + gif->ImageCount; ++pimg) {
        int idx = pimg - gif->SavedImages;
        /* Create image surface */
        gif_img[idx].img = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
        if (cairo_surface_status(gif_img[idx].img) != CAIRO_STATUS_SUCCESS) {
            fprintf(stderr, "Could not create surface: %s\n",
                    cairo_status_to_string(cairo_surface_status(gif_img[idx].img)));
            free(gif_img);
            gif_img = NULL;
            goto read_gif_image_clean;
        }
        cairo_surface_flush(gif_img[idx].img);
        uint32_t *data = (uint32_t *)cairo_image_surface_get_data(gif_img[idx].img);

        // TODO: Check L,R,W,H && color map?
        /* Find Graphics Control extension and delay */
        for (int iext = 0; iext < pimg->ExtensionBlockCount; ++iext) {
            ExtensionBlock *pext = pimg->ExtensionBlocks + iext;
            switch (pext->Function) {
                case GRAPHICS_EXT_FUNC_CODE:
                    DGifExtensionToGCB(pext->ByteCount, pext->Bytes, &gc);
                    // Delay time is in [ms]. Scale it to seconds.
                    gif_img[idx].delay_sec = gc.DelayTime * 0.01;
                    break;
                default:
                    gif_img[idx].delay_sec = 0;
            }
        }

        /* Set general image attributes */
        ColorMapObject *cmap_img = pimg->ImageDesc.ColorMap;
        if (!cmap_img) cmap_img = cmap;
        int img_left = pimg->ImageDesc.Left;
        int img_width = pimg->ImageDesc.Width;
        int img_right = img_left + img_width;
        int img_top = pimg->ImageDesc.Top;
        int img_bottom = img_top + pimg->ImageDesc.Height;

        /* Handle disposal mode */
        int data_size = width * height * ((int)sizeof(width));
        switch (gc.DisposalMode) {
            case DISPOSE_DO_NOT:
                if (data_prev) {
                    memcpy(data, data_prev, data_size);
                } else {
                    memset(data, 0, data_size);
                }
                break;
            case DISPOSE_BACKGROUND:
                memset(data, bg_color, data_size);
                break;
            default:
                memset(data, 0, data_size);
        }

        /* Read RGB */
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                int color_idx;
                if (x < img_left || x >= img_right || y < img_top || y >= img_bottom) {
                    color_idx = bg_idx;
                } else {
                    color_idx = *(pimg->RasterBits + (x - img_left) + ((y - img_top) * img_width));
                }

                if (color_idx != gc.TransparentColor) {
                    GifColorType *cmap_rgb = cmap_img->Colors + color_idx;
                    uint32_t rgb = cmap_rgb->Blue | cmap_rgb->Green << 8 | cmap_rgb->Red << 16;
                    data[x + (y*width)] = rgb;
                }
            }
        }
        data_prev = data;

        cairo_surface_mark_dirty(gif_img[idx].img);
    }

read_gif_image_clean:
    if (!DGifCloseFile(gif, &err)) {
        DEBUG("DGifCloseFile call failed, (Error %d)\n", err);
    }

    return gif_img[0].img;
}

static cairo_surface_t *read_raw_image(const char *image_path, const char *image_raw_format) {
    cairo_surface_t *img;

#define RAW_PIXFMT_MAXLEN 6
#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)
    /* Parse format as <width>x<height>:<pixfmt> */
    char pixfmt[RAW_PIXFMT_MAXLEN + 1];
    size_t w, h;
    const char *fmt = "%zux%zu:%" STRINGIFY(RAW_PIXFMT_MAXLEN) "s";
    if (sscanf(image_raw_format, fmt, &w, &h, pixfmt) != 3) {
        fprintf(stderr, "Invalid image format: \"%s\"\n", image_raw_format);
        return NULL;
    }
#undef RAW_PIXFMT_MAXLEN
#undef STRINGIFY1
#undef STRINGIFY

    /* Create image surface */
    img = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Could not create surface: %s\n",
                cairo_status_to_string(cairo_surface_status(img)));
        return NULL;
    }
    cairo_surface_flush(img);

    /* Use uint32_t* because cairo uses native endianness */
    uint32_t *data = (uint32_t *)cairo_image_surface_get_data(img);
    const int pixstride = cairo_image_surface_get_stride(img) / 4;

    FILE *f = fopen(image_path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Could not open image \"%s\": %s\n",
                image_path, strerror(errno));
        cairo_surface_destroy(img);
        return NULL;
    }

    /* Read the image, respecting cairo's stride, according to the pixfmt */
    ssize_t size, count;
    if (strcmp(pixfmt, "native") == 0) {
        /* If the pixfmt is 'native', just read each line directly into the buffer */
        size = w * h * 4;
        count = read_raw_image_native(data, f, w, h, pixstride);
    } else {
        const struct raw_pixel_format *fmt = NULL;

        if (strcmp(pixfmt, "rgb") == 0)
            fmt = &raw_fmt_rgb;
        else if (strcmp(pixfmt, "rgbx") == 0)
            fmt = &raw_fmt_rgbx;
        else if (strcmp(pixfmt, "xrgb") == 0)
            fmt = &raw_fmt_xrgb;
        else if (strcmp(pixfmt, "bgr") == 0)
            fmt = &raw_fmt_bgr;
        else if (strcmp(pixfmt, "bgrx") == 0)
            fmt = &raw_fmt_bgrx;
        else if (strcmp(pixfmt, "xbgr") == 0)
            fmt = &raw_fmt_xbgr;

        if (fmt == NULL) {
            fprintf(stderr, "Unknown raw pixel format: %s\n", pixfmt);
            fclose(f);
            cairo_surface_destroy(img);
            return NULL;
        }

        size = w * h * fmt->bpp;
        count = read_raw_image_fmt(data, f, w, h, pixstride, *fmt);
    }

    cairo_surface_mark_dirty(img);

    if (count < size) {
        if (count < 0 || ferror(f)) {
            fprintf(stderr, "Failed to read image \"%s\": %s\n",
                    image_path, strerror(errno));
            fclose(f);
            cairo_surface_destroy(img);
            return NULL;
        } else {
            /* Print a warning if the file contains less data than expected,
             * but don't abort. It's useful to see how the image looks even if it's wrong. */
            fprintf(stderr, "Warning: expected to read %zi bytes from \"%s\", read %zi\n",
                    size, image_path, count);
        }
    }

    fclose(f);
    return img;
}

static bool verify_png_image(FILE *png_file) {
    unsigned char png_header[8];
    memset(png_header, '\0', sizeof(png_header));
    fseek(png_file, 0, SEEK_SET);
    int bytes_read = fread(png_header, 1, sizeof(png_header), png_file);
    if (bytes_read != sizeof(png_header)) {
        DEBUG("Could not read PNG header\n");
        return false;
    }

    // Check PNG header according to the specification, available at:
    // https://www.w3.org/TR/2003/REC-PNG-20031110/#5PNG-file-signature
    static unsigned char PNG_REFERENCE_HEADER[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (memcmp(PNG_REFERENCE_HEADER, png_header, sizeof(png_header)) != 0) {
        return false;
    }

    return true;
}

static bool verify_gif_image(FILE *gif_file) {
    unsigned char gif_header[6] = {0};
    fseek(gif_file, 0, SEEK_SET);
    int bytes_read = fread(gif_header, 1, sizeof(gif_header), gif_file);
    if (bytes_read != sizeof(gif_header)) {
        fprintf(stderr, "Could not read GIF header\n");
        return false;
    }

    // Check GIF header according to the specification, available at:
    // https://www.w3.org/Graphics/GIF/spec-gif89a.txt
    static const unsigned char GIF_REFERENCE_HEADER[] = {'G', 'I', 'F'};
    if (memcmp(GIF_REFERENCE_HEADER, gif_header, sizeof(GIF_REFERENCE_HEADER)) == 0) {
        if (gif_header[3] >= '0' && gif_header[3] <= '9' &&
            gif_header[4] >= '0' && gif_header[4] <= '9' &&
            gif_header[5] >= 'a' && gif_header[5] <= 'z') {

            return true;
        }
    }

    return false;
}

static enum IMAGE_FORMAT verify_image(const char *image_path) {
    if (!image_path) {
        return false;
    }

    if (image_raw_format != NULL) {
        return IMAGE_FORMAT_RAW;
    }

    /* Check file exists and has correct header */
    FILE *file = fopen(image_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Image file path \"%s\" cannot be opened (Error %s)\n", image_path, strerror(errno));
        return false;
    }

    enum IMAGE_FORMAT format = IMAGE_FORMAT_UNKNOWN;
    if (image_raw_format != NULL) {
        format = IMAGE_FORMAT_RAW;
    } else if (verify_png_image(file)) {
        format = IMAGE_FORMAT_PNG;
    } else if (file_is_jpg(file)) {
        format = IMAGE_FORMAT_JPG;
    } else if (verify_gif_image(file)) {
        format = IMAGE_FORMAT_GIF;
    }

    fclose(file);
    return format;
}

#ifndef __OpenBSD__
/*
 * Callback function for PAM. We only react on password request callbacks.
 *
 */
static int conv_callback(int num_msg, const struct pam_message **msg,
                         struct pam_response **resp, void *appdata_ptr) {
    if (num_msg == 0)
        return 1;

    /* PAM expects an array of responses, one for each message */
    if ((*resp = calloc(num_msg, sizeof(struct pam_response))) == NULL) {
        perror("calloc");
        return 1;
    }

    for (int c = 0; c < num_msg; c++) {
        if (msg[c]->msg_style != PAM_PROMPT_ECHO_OFF &&
            msg[c]->msg_style != PAM_PROMPT_ECHO_ON)
            continue;

        /* return code is currently not used but should be set to zero */
        resp[c]->resp_retcode = 0;
        if ((resp[c]->resp = strdup(password)) == NULL) {
            perror("strdup");
            return 1;
        }
    }

    return 0;
}
#endif

/*
 * This callback is only a dummy, see xcb_prepare_cb and xcb_check_cb.
 * See also man libev(3): "ev_prepare" and "ev_check" - customise your event loop
 *
 */
static void xcb_got_event(EV_P_ struct ev_io *w, int revents) {
    /* empty, because xcb_prepare_cb and xcb_check_cb are used */
}

/*
 * Flush before blocking (and waiting for new events)
 *
 */
static void xcb_prepare_cb(EV_P_ ev_prepare *w, int revents) {
    xcb_flush(conn);
}

/*
 * Try closing logind sleep lock fd passed over from xss-lock, in case we're
 * being run from there.
 *
 */
static void maybe_close_sleep_lock_fd(void) {
    const char *sleep_lock_fd = getenv("XSS_SLEEP_LOCK_FD");
    char *endptr;
    if (sleep_lock_fd && *sleep_lock_fd != 0) {
        long int fd = strtol(sleep_lock_fd, &endptr, 10);
        if (*endptr == 0) {
            close(fd);
        }
    }
}

/*
 * Instead of polling the X connection socket we leave this to
 * xcb_poll_for_event() which knows better than we can ever know.
 *
 */
static void xcb_check_cb(EV_P_ ev_check *w, int revents) {
    xcb_generic_event_t *event;

    if (xcb_connection_has_error(conn))
        errx(EXIT_FAILURE, "X11 connection broke, did your server terminate?");

    while ((event = xcb_poll_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            xcb_generic_error_t *error = (xcb_generic_error_t *)event;
            if (debug_mode)
                fprintf(stderr, "X11 Error received! sequence 0x%x, error_code = %d\n",
                        error->sequence, error->error_code);
            free(event);
            continue;
        }

        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);

        switch (type) {
            case XCB_KEY_PRESS:
                handle_key_press((xcb_key_press_event_t *)event);
                break;

            case XCB_VISIBILITY_NOTIFY:
                handle_visibility_notify(conn, (xcb_visibility_notify_event_t *)event);
                break;

            case XCB_MAP_NOTIFY:
                maybe_close_sleep_lock_fd();
                if (!dont_fork) {
                    /* After the first MapNotify, we never fork again. We don’t
                     * expect to get another MapNotify, but better be sure… */
                    dont_fork = true;

                    /* In the parent process, we exit */
                    if (fork() != 0)
                        exit(EXIT_SUCCESS);

                    ev_loop_fork(EV_DEFAULT);
                }
                break;

            case XCB_CONFIGURE_NOTIFY:
                handle_screen_resize();
                break;

            default:
                if (type == xkb_base_event) {
                    process_xkb_event(event);
                }
                if (randr_base > -1 &&
                    type == randr_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
                    randr_query(screen->root);
                    handle_screen_resize();
                }
        }

        free(event);
    }
}

/*
 * This function is called from a fork()ed child and will raise the i3lock
 * window when the window is obscured, even when the main i3lock process is
 * blocked due to the authentication backend.
 *
 */
static void raise_loop(xcb_window_t window) {
    xcb_connection_t *conn;
    xcb_generic_event_t *event;
    int screens;

    if (xcb_connection_has_error((conn = xcb_connect(NULL, &screens))) > 0)
        errx(EXIT_FAILURE, "Cannot open display");

    /* We need to know about the window being obscured or getting destroyed. */
    xcb_change_window_attributes(conn, window, XCB_CW_EVENT_MASK,
                                 (uint32_t[]){
                                     XCB_EVENT_MASK_VISIBILITY_CHANGE |
                                     XCB_EVENT_MASK_STRUCTURE_NOTIFY});
    xcb_flush(conn);

    DEBUG("Watching window 0x%08x\n", window);
    while ((event = xcb_wait_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            xcb_generic_error_t *error = (xcb_generic_error_t *)event;
            DEBUG("X11 Error received! sequence 0x%x, error_code = %d\n",
                  error->sequence, error->error_code);
            free(event);
            continue;
        }
        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);
        DEBUG("Read event of type %d\n", type);
        switch (type) {
            case XCB_VISIBILITY_NOTIFY:
                handle_visibility_notify(conn, (xcb_visibility_notify_event_t *)event);
                break;
            case XCB_UNMAP_NOTIFY:
                DEBUG("UnmapNotify for 0x%08x\n", (((xcb_unmap_notify_event_t *)event)->window));
                if (((xcb_unmap_notify_event_t *)event)->window == window)
                    exit(EXIT_SUCCESS);
                break;
            case XCB_DESTROY_NOTIFY:
                DEBUG("DestroyNotify for 0x%08x\n", (((xcb_destroy_notify_event_t *)event)->window));
                if (((xcb_destroy_notify_event_t *)event)->window == window)
                    exit(EXIT_SUCCESS);
                break;
            default:
                DEBUG("Unhandled event type %d\n", type);
                break;
        }
        free(event);
    }
}

/*
 * Loads an image from the given path. Handles JPEG and PNG. Returns NULL in case of error.
 */
cairo_surface_t *load_image(enum IMAGE_FORMAT format) {
    cairo_surface_t *img = NULL;
    JPEG_INFO jpg_info;
    unsigned char *jpg_data;

    switch (format) {
        case IMAGE_FORMAT_RAW:
            /* Read image. 'read_raw_image' returns NULL on error,
             * so we don't have to handle errors here. */
            img = read_raw_image(image_path, image_raw_format);
            break;
        case IMAGE_FORMAT_PNG:
            img = cairo_image_surface_create_from_png(image_path);
            break;
        case IMAGE_FORMAT_JPG:
            jpg_data = read_JPEG_file(image_path, &jpg_info);
            if (jpg_data != NULL) {
                img = cairo_image_surface_create_for_data(jpg_data,
                                                          CAIRO_FORMAT_ARGB32, jpg_info.width, jpg_info.height,
                                                          jpg_info.stride);
            }
            break;
        case IMAGE_FORMAT_GIF:
            img = read_gif_image(image_path);
            break;
        default:
            fprintf(stderr, "Unsupported image file format: %s\n", image_path);
    }

    /* In case loading failed, we just pretend no -i was specified. */
    if (img && cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Could not load image, %s\n",
                cairo_status_to_string(cairo_surface_status(img)));
        img = NULL;
    }

    return img;
}

/*
 * Reads the provided directory and stores the images path in the pointer array
 * img_slideshow
 */
bool load_slideshow_images(const char *path) {
    slideshow_enabled = true;
    DIR *d;
    struct dirent *dir;
    int file_count = 0;
    slideshow_image_count = 0;

    DEBUG("Loading slideshow images at \"%s\"\n", path);

    d = opendir(path);
    if (d == NULL) {
        printf("Could not open directory: %s\n", path);
        return false;
    }

    regex_t reg;

    if (regcomp(&reg, ".*\\.(jpe?g|png)", REG_EXTENDED)) {
        printf("Could not compile regex\n");
        return false;
    }

    while ((dir = readdir(d)) != NULL) {
        if (file_count >= 256) break;
        int result = regexec(&reg, dir->d_name, 0, NULL, 0);
        if (result) continue;

        char path_to_image[256];
        strcpy(path_to_image, path);
        strcat(path_to_image, "/");
        strcat(path_to_image, dir->d_name);

        img_slideshow[file_count] = strdup(path_to_image);

        if (img_slideshow[file_count] != NULL) {
            ++file_count;
        }
    }

    slideshow_image_count = file_count;
    regfree(&reg);
    closedir(d);
    return true;
}

void gif_anim_loop(struct ev_loop *loop, struct ev_timer *timer, int delay) {
    static int img_count = 0;

    if (++img_count >= gif_img_count) img_count = 0;
    img = gif_img[img_count].img;
    ev_timer_stop(loop, timer);
    redraw_screen();
    ev_timer_set(timer, gif_img[img_count].delay_sec, 0.);
    ev_timer_start(loop, timer);
}

int main(int argc, char *argv[]) {
    struct passwd *pw;
    char *username;
#ifndef __OpenBSD__
    int ret;
    struct pam_conv conv = {conv_callback, NULL};
#endif
    int curs_choice = CURS_NONE;
    int o;
    int longoptind = 0;
    struct option longopts[] = {
        {"version", no_argument, NULL, 'v'},
        {"nofork", no_argument, NULL, 'n'},
        {"beep", no_argument, NULL, 'b'},
        {"dpms", no_argument, NULL, 'd'},
        {"color", required_argument, NULL, 'c'},
        {"pointer", required_argument, NULL, 'p'},
        {"debug", no_argument, NULL, 999},
        {"help", no_argument, NULL, 'h'},
        {"no-unlock-indicator", no_argument, NULL, 'u'},
        {"image", required_argument, NULL, 'i'},
        {"raw", required_argument, NULL, 998},
        {"tiling", no_argument, NULL, 't'},
        {"centered", no_argument, NULL, 'C'},
        {"fill", no_argument, NULL, 'F'},
        {"scale", no_argument, NULL, 'L'},
        {"max", no_argument, NULL, 'M'},
        {"ignore-empty-password", no_argument, NULL, 'e'},
        {"inactivity-timeout", required_argument, NULL, 'I'},
        {"show-failed-attempts", no_argument, NULL, 'f'},
        {"screen", required_argument, NULL, 'S'},
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

		// pass keys
        {"pass-media-keys", no_argument, NULL, 601},
        {"pass-screen-keys", no_argument, NULL, 602},
        {"pass-power-keys", no_argument, NULL, 603},
        {"pass-volume-keys", no_argument, NULL, 604},

        // custom commands for pass keys
        {"custom-key-commands", no_argument, NULL, 610},
        {"cmd-brightness-up", required_argument, NULL, 620},
        {"cmd-brightness-down", required_argument, NULL, 621},

        {"cmd-media-play", required_argument, NULL, 630},
        {"cmd-media-pause", required_argument, NULL, 631},
        {"cmd-media-stop", required_argument, NULL, 632},
        {"cmd-media-next", required_argument, NULL, 633},
        {"cmd-media-prev", required_argument, NULL, 634},

        {"cmd-audio-mute", required_argument, NULL, 640},
        {"cmd-volume-up", required_argument, NULL, 641},
        {"cmd-volume-down", required_argument, NULL, 642},
        {"cmd-mic-mute", required_argument, NULL, 643},

        {"cmd-power-down", required_argument, NULL, 650},
        {"cmd-power-off", required_argument, NULL, 651},
        {"cmd-power-sleep", required_argument, NULL, 652},

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

        // misc.
        {"redraw-thread", no_argument, NULL, 900},
        {"refresh-rate", required_argument, NULL, 901},
        {"composite", no_argument, NULL, 902},
        {"no-verify", no_argument, NULL, 905},

        // slideshow options
        {"slideshow-interval", required_argument, NULL, 903},
        {"slideshow-random-selection", no_argument, NULL, 904},

        {NULL, no_argument, NULL, 0}};

    if ((pw = getpwuid(getuid())) == NULL)
        err(EXIT_FAILURE, "getpwuid() failed");
    if ((username = pw->pw_name) == NULL)
        errx(EXIT_FAILURE, "pw->pw_name is NULL.");
    if (getenv("WAYLAND_DISPLAY") != NULL)
        errx(EXIT_FAILURE, "i3lock is a program for X11 and does not work on Wayland. Try https://github.com/swaywm/swaylock instead");

    char *optstring = "hvnbdc:p:ui:tCFLMeI:frsS:kB:m";
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
    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", acolor) != 1)\
        errx(1, #acolor " is invalid, color must be given in 3 or 4-byte format: rrggbb[aa]\n");

#define parse_outline_width(awidth)\
    arg = optarg;\
    if (sscanf(arg, "%lf", &awidth) != 1)\
        errx(1, #awidth " must be a number\n");\
    if (awidth < 0) {\
        fprintf(stderr, #awidth " must be a positive double; ignoring...\n");\
        awidth = 0;\
    }

    while ((o = getopt_long(argc, argv, optstring, longopts, &longoptind)) != -1) {
        switch (o) {
            case 'v':
                errx(EXIT_SUCCESS, "version " I3LOCK_VERSION " © 2010 Michael Stapelberg, © 2015 Cassandra Fox, © 2021 Raymond Li");
            case 'n':
                dont_fork = true;
                break;
            case 'b':
                beep = true;
                break;
            case 'd':
                fprintf(stderr, "DPMS support has been removed from i3lock. Please see the manpage i3lock(1).\n");
                break;
            case 'I': {
                fprintf(stderr, "Inactivity timeout only makes sense with DPMS, which was removed. Please see the manpage i3lock(1).\n");
                break;
            }
            case 'u':
                unlock_indicator = false;
                break;
            case 'i':
                image_path = strdup(optarg);
                break;
            case 't':
                if(bg_type != NONE) {
                    errx(EXIT_FAILURE, "i3lock-color: Only one background type can be used.");
                }
                bg_type = TILE;
                break;
            case 'C':
                if(bg_type != NONE) {
                    errx(EXIT_FAILURE, "i3lock-color: Only one background type can be used.");
                }
                bg_type = CENTER;
                break;
            case 'F':
                if(bg_type != NONE) {
                    errx(EXIT_FAILURE, "i3lock-color: Only one background type can be used.");
                }
                bg_type = FILL;
                break;
            case 'L':
                if(bg_type != NONE) {
                    errx(EXIT_FAILURE, "i3lock-color: Only one background type can be used.");
                }
                bg_type = SCALE;
                break;
            case 'M':
                if(bg_type != NONE) {
                    errx(EXIT_FAILURE, "i3lock-color: Only one background type can be used.");
                }
                bg_type = MAX;
                break;
            case 'p':
                if (!strcmp(optarg, "win")) {
                    curs_choice = CURS_WIN;
                } else if (!strcmp(optarg, "default")) {
                    curs_choice = CURS_DEFAULT;
                } else {
                    errx(EXIT_FAILURE, "i3lock: Invalid pointer type given. Expected one of \"win\" or \"default\".");
                }
                break;
            case 'e':
                ignore_empty_password = true;
                break;
            case 'f':
                show_failed_attempts = true;
                break;
            case 'r':
                if (internal_line_source != 0) {
                  errx(EXIT_FAILURE, "i3lock-color: Options line-uses-ring and line-uses-inside conflict.");
                }
                internal_line_source = 1; //sets the line drawn inside to use the inside color when drawn
                break;
            case 's':
                if (internal_line_source != 0) {
                  errx(EXIT_FAILURE, "i3lock-color: Options line-uses-ring and line-uses-inside conflict.");
                }
                internal_line_source = 2;
                break;
            case 'S':
                screen_number = atoi(optarg);
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
                if (sscanf(arg, "%lf", &circle_radius) != 1)
                    errx(1, "radius must be a number\n");
                if (circle_radius < 1) {
                    fprintf(stderr, "radius must be a positive integer; ignoring...\n");
                    circle_radius = 90.0;
                }
                break;
            case 403:
                arg = optarg;
                if (sscanf(arg, "%lf", &ring_width) != 1)
                    errx(1, "ring-width must be a number\n");
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
                    errx(1, "time format string can be at most 31 characters\n");
                }
                strcpy(time_format,optarg);
                break;
            case 511:
                if (strlen(optarg) > 31) {
                    errx(1, "time format string can be at most 31 characters\n");
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
                    errx(1, "time font string can be at most 63 characters\n");
                }
                strcpy(fonts[TIME_FONT],optarg);
                break;
            case 521:
                if (strlen(optarg) > 63) {
                    errx(1, "date font string can be at most 63 characters\n");
                }
                strcpy(fonts[DATE_FONT],optarg);
                break;
            case 522:
                if (strlen(optarg) > 63) {
                    errx(1, "verif font string can be at most 63 "
                            "characters\n");
                }
                strcpy(fonts[VERIF_FONT],optarg);
                break;
            case 523:
                if (strlen(optarg) > 63) {
                    errx(1, "wrong font string can be at most 63 "
                            "characters\n");
                }
                strcpy(fonts[WRONG_FONT],optarg);
                break;
            case 524:
                if (strlen(optarg) > 63) {
                    errx(1, "layout font string can be at most 63 characters\n");
                }
                strcpy(fonts[LAYOUT_FONT],optarg);
                break;
            case 525:
                if (strlen(optarg) > 63) {
                    errx(1, "greeter font string can be at most 63 characters\n");
                }
                strcpy(fonts[GREETER_FONT],optarg);
                break;

			// Text size
            case 530:
                arg = optarg;
                if (sscanf(arg, "%lf", &time_size) != 1)
                    errx(1, "timesize must be a number\n");
                if (time_size < 1)
                    errx(1, "timesize must be larger than 0\n");
                break;
            case 531:
                arg = optarg;
                if (sscanf(arg, "%lf", &date_size) != 1)
                    errx(1, "datesize must be a number\n");
                if (date_size < 1)
                    errx(1, "datesize must be larger than 0\n");
                break;
            case 532:
                arg = optarg;
                if (sscanf(arg, "%lf", &verif_size) != 1)
                    errx(1, "verifsize must be a number\n");
                if (verif_size < 1) {
                    fprintf(stderr, "verifsize must be a positive integer; ignoring...\n");
                    verif_size = 28.0;
                }
                break;
            case 533:
                arg = optarg;
                if (sscanf(arg, "%lf", &wrong_size) != 1)
                    errx(1, "wrongsize must be a number\n");
                if (wrong_size < 1) {
                    fprintf(stderr, "wrongsize must be a positive integer; ignoring...\n");
                    wrong_size = 28.0;
                }
                break;
            case 534:
                arg = optarg;
                if (sscanf(arg, "%lf", &layout_size) != 1)
                    errx(1, "layoutsize must be a number\n");
                if (date_size < 1)
                    errx(1, "layoutsize must be larger than 0\n");
                break;
            case 535:
                arg = optarg;
                if (sscanf(arg, "%lf", &modifier_size) != 1)
                    errx(1, "modsize must be a number\n");
                if (modifier_size < 1) {
                    fprintf(stderr, "modsize must be a positive integer; ignoring...\n");
                    modifier_size = 14.0;
                }
                break;
            case 536:
                arg = optarg;
                if (sscanf(arg, "%lf", &greeter_size) != 1)
                    errx(1, "greetersize must be a number\n");
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
                    errx(1, "time position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", time_x_expr, time_y_expr) != 2) {
                    errx(1, "timepos must be of the form x:y\n");
                }
                break;
            case 541:
                //read in to date_x_expr and date_y_expr
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    errx(1, "date position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", date_x_expr, date_y_expr) != 2) {
                    errx(1, "datepos must be of the form x:y\n");
                }
                break;
            case 542:
                // read in to time_x_expr and time_y_expr
                if (strlen(optarg) > 31) {
                    errx(1, "verif position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", verif_x_expr, verif_y_expr) != 2) {
                    errx(1, "verifpos must be of the form x:y\n");
                }
                break;
            case 543:
                if (strlen(optarg) > 31) {
                    errx(1, "\"wrong\" text position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", wrong_x_expr, wrong_y_expr) != 2) {
                    errx(1, "wrongpos must be of the form x:y\n");
                }
                break;
            case 544:
                if (strlen(optarg) > 31) {
                    errx(1, "layout position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", layout_x_expr, layout_y_expr) != 2) {
                    errx(1, "layoutpos must be of the form x:y\n");
                }
                break;
            case 545:
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    errx(1, "status position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", status_x_expr, status_y_expr) != 2) {
                    errx(1, "statuspos must be of the form x:y\n");
                }
                break;
            case 546:
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    errx(1, "modif position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", modif_x_expr, modif_y_expr) != 2) {
                    errx(1, "modifpos must be of the form x:y\n");
                }
                break;
            case 547:
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    errx(1, "indicator position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", ind_x_expr, ind_y_expr) != 2) {
                    errx(1, "indpos must be of the form x:y\n");
                }
                break;
            case 548:
                if (strlen(optarg) > 31) {
                    // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                    errx(1, "greeter position string can be at most 31 characters\n");
                }
                arg = optarg;
                if (sscanf(arg, "%30[^:]:%30[^:]", greeter_x_expr, greeter_y_expr) != 2) {
                    errx(1, "greeterpos must be of the form x:y\n");
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


			// Pass keys
			case 601:
				pass_media_keys = true;
				break;
			case 602:
				pass_screen_keys = true;
				break;
			case 603:
				pass_power_keys = true;
				break;
			case 604:
				pass_volume_keys = true;
				break;

            //custom key commands
            case 610:
                hotkeys = true;
                break;
            case 620:
                cmd_brightness_up = optarg;
                break;
            case 621:
                cmd_brightness_down = optarg;
                break;

            case 630:
                cmd_media_play = optarg;
                break;
            case 631:
                cmd_media_pause = optarg;
                break;
            case 632:
                cmd_media_stop = optarg;
                break;
            case 633:
                cmd_media_next = optarg;
                break;
            case 634:
                cmd_media_prev = optarg;
                break;

            case 640:
                cmd_audio_mute = optarg;
                break;
            case 641:
                cmd_volume_up = optarg;
                break;
            case 642:
                cmd_volume_down = optarg;
                break;
            case 643:
                cmd_mic_mute = optarg;
                break;

            case 650:
                cmd_power_down = optarg;
                break;
            case 651:
                cmd_power_off = optarg;
                break;
            case 652:
                cmd_power_sleep = optarg;
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
                    errx(1, "bar orientation must be \"vertical\" or \"horizontal\"\n");
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
                    errx(1, "bar-position must be a single number or of the form x:y with a max length of 31\n");
                }
                break;
            case 710:
                bar_count = atoi(optarg);
                if (bar_count > MAX_BAR_COUNT || bar_count < MIN_BAR_COUNT) {
                    errx(1, "bar-count must be between %d and %d\n", MIN_BAR_COUNT, MAX_BAR_COUNT);
                }
                break;
            case 711:
                arg = optarg;
                if (sscanf(arg, "%31s", bar_width_expr) != 1) {
                    errx(1, "missing argument for bar-total-width\n");
                }
                break;

			// Misc
            case 900:
                redraw_thread = true;
                break;
            case 901:
                arg = optarg;
                refresh_rate = strtof(arg, NULL);
                if (refresh_rate < 0.0) {
                    fprintf(stderr, "The given refresh rate of %fs is less than zero seconds and was ignored.\n", refresh_rate);
                    refresh_rate = 1.0;
                }
                break;
            case 902:
                composite = true;
                break;
            case 903:
                slideshow_interval = atoi(optarg);

                if (slideshow_interval < 0) {
                    slideshow_interval = 10;
                }
                break;
            case 904:
                slideshow_random_selection = true;
                break;
            case 905:
                no_verify = true;
                break;
            case 998:
                image_raw_format = strdup(optarg);
                break;
            case 999:
                debug_mode = true;
                break;
            default:
                errx(EXIT_FAILURE, "Syntax: i3lock [-v] [-n] [-b] [-d] [-c color] [-u] [-p win|default]"
                                   " [-i image.png] [-t] [-e] [-f]\n"
                                   "Please see the manpage for a full list of arguments.");
        }
    }

    /* We need (relatively) random numbers for highlighting a random part of
     * the unlock indicator upon keypresses. */
    srand(time(NULL));

#ifndef __OpenBSD__
    /* Initialize PAM */
    if ((ret = pam_start("i3lock", username, &conv, &pam_handle)) != PAM_SUCCESS)
        errx(EXIT_FAILURE, "PAM: %s", pam_strerror(pam_handle, ret));

    if ((ret = pam_set_item(pam_handle, PAM_TTY, getenv("DISPLAY"))) != PAM_SUCCESS)
        errx(EXIT_FAILURE, "PAM: %s", pam_strerror(pam_handle, ret));
#endif

/* Using mlock() as non-super-user seems only possible in Linux.
 * Users of other operating systems should use encrypted swap/no swap
 * (or remove the ifdef and run i3lock as super-user).
 * Alas, swap is encrypted by default on OpenBSD so swapping out
 * is not necessarily an issue. */
#if defined(__linux__)
    /* Lock the area where we store the password in memory, we don’t want it to
     * be swapped to disk. Since Linux 2.6.9, this does not require any
     * privileges, just enough bytes in the RLIMIT_MEMLOCK limit. */
    if (mlock(password, sizeof(password)) != 0)
        err(EXIT_FAILURE, "Could not lock page in memory, check RLIMIT_MEMLOCK");
#endif

    /* Double checking that connection is good and operatable with xcb */
    int screennr;
    if ((conn = xcb_connect(NULL, &screennr)) == NULL ||
        xcb_connection_has_error(conn))
            errx(EXIT_FAILURE, "Could not connect to X11, maybe you need to set DISPLAY?");

    if (xkb_x11_setup_xkb_extension(conn,
                                    XKB_X11_MIN_MAJOR_XKB_VERSION,
                                    XKB_X11_MIN_MINOR_XKB_VERSION,
                                    0,
                                    NULL,
                                    NULL,
                                    &xkb_base_event,
                                    &xkb_base_error) != 1)
        errx(EXIT_FAILURE, "Could not setup XKB extension.");

    layout_text = get_keylayoutname(keylayout_mode, conn);
    if (layout_text)
        show_clock = true;
    static const xcb_xkb_map_part_t required_map_parts =
        (XCB_XKB_MAP_PART_KEY_TYPES |
         XCB_XKB_MAP_PART_KEY_SYMS |
         XCB_XKB_MAP_PART_MODIFIER_MAP |
         XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS |
         XCB_XKB_MAP_PART_KEY_ACTIONS |
         XCB_XKB_MAP_PART_VIRTUAL_MODS |
         XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP);

    static const xcb_xkb_event_type_t required_events =
        (XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY |
         XCB_XKB_EVENT_TYPE_MAP_NOTIFY |
         XCB_XKB_EVENT_TYPE_STATE_NOTIFY);

    xcb_xkb_select_events(
        conn,
        xkb_x11_get_core_keyboard_device_id(conn),
        required_events,
        0,
        required_events,
        required_map_parts,
        required_map_parts,
        0);

    /* When we cannot initially load the keymap, we better exit */
    if (!load_keymap())
        errx(EXIT_FAILURE, "Could not load keymap");


    const char *locale = getenv("LC_ALL");
    if (!locale || !*locale)
        locale = getenv("LC_TIME");
    if (!locale || !*locale)
        locale = getenv("LC_CTYPE");
    if (!locale || !*locale)
        locale = getenv("LANG");
    if (!locale || !*locale) {
        if (debug_mode)
            fprintf(stderr, "Can't detect your locale, fallback to C\n");
        locale = "C";
    }

    setlocale(LC_ALL, locale);

#if XKBCOMPOSE == 1
    load_compose_table(locale);
#endif


    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    init_dpi();

    randr_init(&randr_base, screen->root);
    randr_query(screen->root);

    last_resolution[0] = screen->width_in_pixels;
    last_resolution[1] = screen->height_in_pixels;

    if (bar_enabled) {
        bar_heights = (double*) calloc(bar_count, sizeof(double));
    }

    xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK,
                                 (uint32_t[]){XCB_EVENT_MASK_STRUCTURE_NOTIFY});

    init_colors_once();
    if (image_path != NULL) {
        if (!is_directory(image_path)) {
            enum IMAGE_FORMAT image_format = verify_image(image_path);
            img = load_image(image_format);
        } else {
            /* Path to a directory is provided -> use slideshow mode */
            slideshow_path = strdup(image_path);
            if (!load_slideshow_images(slideshow_path)) exit(EXIT_FAILURE);
            enum IMAGE_FORMAT image_format = verify_image(img_slideshow[0]);
            img = load_image(image_format);
        }
        free(image_path);
    }
    free(image_raw_format);

    if (blur) {
        xcb_pixmap_t bg_pixmap = capture_bg_pixmap(conn, screen, last_resolution);
        cairo_surface_t *xcb_img = cairo_xcb_surface_create(conn, bg_pixmap, get_root_visual_type(screen), last_resolution[0], last_resolution[1]);

        blur_bg_img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, last_resolution[0], last_resolution[1]);
        cairo_t *ctx = cairo_create(blur_bg_img);

        cairo_set_source_surface(ctx, xcb_img, 0, 0);
        cairo_paint(ctx);
        blur_image_surface(blur_bg_img, blur_sigma);

        cairo_destroy(ctx);
        cairo_surface_destroy(xcb_img);
        xcb_free_pixmap(conn, bg_pixmap);
    }

    xcb_window_t stolen_focus = find_focused_window(conn, screen->root);

    /* Open the fullscreen window, already with the correct pixmap in place */
    win = open_fullscreen_window(conn, screen, color);

    xcb_pixmap_t pixmap = create_bg_pixmap(conn, win, last_resolution, color);
    render_lock(last_resolution, pixmap);
    xcb_change_window_attributes(conn, win, XCB_CW_BACK_PIXMAP, (uint32_t[]){pixmap});
    xcb_free_pixmap(conn, pixmap);

    cursor = create_cursor(conn, screen, win, curs_choice);

    /* Display the "locking…" message while trying to grab the pointer/keyboard. */
    auth_state = STATE_AUTH_LOCK;
    if (!grab_pointer_and_keyboard(conn, screen, cursor, 1000)) {
        DEBUG("stole focus from X11 window 0x%08x\n", stolen_focus);

        /* Set the focus to i3lock, possibly closing context menus which would
         * otherwise prevent us from grabbing keyboard/pointer.
         *
         * We cannot use set_focused_window because _NET_ACTIVE_WINDOW only
         * works for managed windows, but i3lock uses an unmanaged window
         * (override_redirect=1). */
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_PARENT /* revert_to */, win, XCB_CURRENT_TIME);
        if (!grab_pointer_and_keyboard(conn, screen, cursor, 9000)) {
            auth_state = STATE_I3LOCK_LOCK_FAILED;
            redraw_screen();
            sleep(1);
            errx(EXIT_FAILURE, "Cannot grab pointer/keyboard");
        }
    }

    pid_t pid = fork();
    /* The pid == -1 case is intentionally ignored here:
     * While the child process is useful for preventing other windows from
     * popping up while i3lock blocks, it is not critical. */
    if (pid == 0) {
        /* Child */
        close(xcb_get_file_descriptor(conn));
        maybe_close_sleep_lock_fd();
        raise_loop(win);
        exit(EXIT_SUCCESS);
    }

    /* Load the keymap again to sync the current modifier state. Since we first
     * loaded the keymap, there might have been changes, but starting from now,
     * we should get all key presses/releases due to having grabbed the
     * keyboard. */
    (void)load_keymap();

    /* Initialize the libev event loop. */
    main_loop = EV_DEFAULT;
    if (main_loop == NULL)
        errx(EXIT_FAILURE, "Could not initialize libev. Bad LIBEV_FLAGS?");

    /* Explicitly call the screen redraw in case "locking…" message was displayed */
    auth_state = STATE_AUTH_IDLE;
    redraw_screen();

    struct ev_io *xcb_watcher = calloc(sizeof(struct ev_io), 1);
    struct ev_check *xcb_check = calloc(sizeof(struct ev_check), 1);
    struct ev_prepare *xcb_prepare = calloc(sizeof(struct ev_prepare), 1);
    struct ev_timer *xcb_timer = calloc(sizeof(struct ev_timer), 1);

    ev_io_init(xcb_watcher, xcb_got_event, xcb_get_file_descriptor(conn), EV_READ);
    ev_io_start(main_loop, xcb_watcher);

    ev_check_init(xcb_check, xcb_check_cb);
    ev_check_start(main_loop, xcb_check);

    ev_prepare_init(xcb_prepare, xcb_prepare_cb);
    ev_prepare_start(main_loop, xcb_prepare);

    if (gif_img) {
        ev_timer_init(xcb_timer, gif_anim_loop, gif_img[0].delay_sec, 0.);
        ev_timer_start(main_loop, xcb_timer);
    }

    /* Invoke the event callback once to catch all the events which were
     * received up until now. ev will only pick up new events (when the X11
     * file descriptor becomes readable). */
    ev_invoke(main_loop, xcb_check, 0);

    if (show_clock || bar_enabled || slideshow_enabled) {
        if (redraw_thread) {
            struct timespec ts;
            double s;
            double ns = modf(refresh_rate, &s);
            ts.tv_sec = (time_t) s;
            ts.tv_nsec = ns * NANOSECONDS_IN_SECOND;
            (void) pthread_create(&draw_thread, NULL, start_time_redraw_tick_pthread, (void*) &ts);
        } else {
            start_time_redraw_tick(main_loop);
        }
    }
    ev_loop(main_loop, 0);

#ifndef __OpenBSD__
    if (pam_cleanup) {
        pam_end(pam_handle, PAM_SUCCESS);
    }
#endif

    if (stolen_focus == XCB_NONE) {
        return 0;
    }

    DEBUG("restoring focus to X11 window 0x%08x\n", stolen_focus);
    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
    xcb_destroy_window(conn, win);
    set_focused_window(conn, screen->root, stolen_focus);
    xcb_aux_sync(conn);

    return 0;
}
