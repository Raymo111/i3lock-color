/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2010 Michael Stapelberg
 *
 * See LICENSE for licensing information
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <err.h>
#include <assert.h>
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
#ifdef __OpenBSD__
#include <strings.h> /* explicit_bzero(3) */
#endif

#include "i3lock.h"
#include "xcb.h"
#include "cursors.h"
#include "unlock_indicator.h"
#include "xinerama.h"
#include "blur.h"

#define TSTAMP_N_SECS(n) (n * 1.0)
#define TSTAMP_N_MINS(n) (60 * TSTAMP_N_SECS(n))
#define START_TIMER(timer_obj, timeout, callback) \
    timer_obj = start_timer(timer_obj, timeout, callback)
#define STOP_TIMER(timer_obj) \
    timer_obj = stop_timer(timer_obj)

typedef void (*ev_callback_t)(EV_P_ ev_timer *w, int revents);
static void input_done(void);

char color[7] = "ffffff";

/* options for unlock indicator colors */
char insidevercolor[9] = "006effbf";
char insidewrongcolor[9] = "fa0000bf";
char insidecolor[9] = "000000bf";
char ringvercolor[9] = "3300faff";
char ringwrongcolor[9] = "7d3300ff";
char ringcolor[9] = "337d00ff";
char linecolor[9] = "000000ff";
char textcolor[9] = "000000ff";
char timecolor[9] = "000000ff";
char datecolor[9] = "000000ff";
char keyhlcolor[9] = "33db00ff";
char bshlcolor[9] = "db3300ff";
char separatorcolor[9] = "000000ff";

/* int defining which display the lock indicator should be shown on. If -1, then show on all displays.*/
int screen_number = -1;
/* default is to use the supplied line color, 1 will be ring color, 2 will be to use the inside color for ver/wrong/etc */
int internal_line_source = 0;
/* bool for showing the clock; why am I commenting this? */
bool show_clock = false;
bool show_indicator = false;
float refresh_rate = 1.0;

/* there's some issues with compositing - upstream removed support for this, but we'll allow people to supply an arg to enable it */ 
bool composite = false;
/* time formatter strings for date/time
    I picked 32-length char arrays because some people might want really funky time formatters.
    Who am I to judge?
*/
char time_format[32] = "%H:%M:%S\0";
char date_format[32] = "%A, %m %Y\0";
char time_font[32] = "sans-serif\0";
char date_font[32] = "sans-serif\0";
char time_x_expr[32] = "ix - (cw / 2)\0";
char time_y_expr[32] = "iy - (ch / 2)\0";
char date_x_expr[32] = "tx\0";
char date_y_expr[32] = "ty+30\0";

double time_size = 32.0;
double date_size = 14.0;
double text_size = 28.0;
double modifier_size = 14.0;
double circle_radius = 90.0;

char* verif_text = "verifying…";
char* wrong_text = "wrong!";

/* opts for blurring */
bool blur = false;
bool step_blur = false;
int blur_sigma = 5;

uint32_t last_resolution[2];
xcb_window_t win;
static xcb_cursor_t cursor;
#ifndef __OpenBSD__
static pam_handle_t *pam_handle;
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

cairo_surface_t *img = NULL;
cairo_surface_t *blur_img = NULL;
bool tile = false;
bool ignore_empty_password = false;
bool skip_repeated_empty_password = false;

/* isutf, u8_dec © 2005 Jeff Bezanson, public domain */
#define isutf(c) (((c)&0xC0) != 0x80)

/*
 * Decrements i to point to the previous unicode glyph
 *
 */
void u8_dec(char *s, int *i) {
    (void)(isutf(s[--(*i)]) || isutf(s[--(*i)]) || isutf(s[--(*i)]) || --(*i));
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
#ifdef __OpenBSD__
    /* Use explicit_bzero(3) which was explicitly designed not to be
     * optimized out by the compiler. */
    explicit_bzero(password, strlen(password));
#else
    /* A volatile pointer to the password buffer to prevent the compiler from
     * optimizing this out. */
    volatile char *vpassword = password;
    for (int c = 0; c < sizeof(password); c++)
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

#ifdef __OpenBSD__
    struct passwd *pw;

    if (!(pw = getpwuid(getuid())))
        errx(1, "unknown uid %u.", getuid());

    if (auth_userokay(pw->pw_name, NULL, NULL, password) != 0) {
        DEBUG("successfully authenticated\n");
        clear_password_memory();

        exit(0);
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
        pam_end(pam_handle, PAM_SUCCESS);

        exit(0);
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
            mod_name = "Win";

        char *tmp;
        if (modifier_string == NULL) {
            if (asprintf(&tmp, "%s", mod_name) != -1)
                modifier_string = tmp;
        } else if (asprintf(&tmp, "%s, %s", modifier_string, mod_name) != -1) {
            free(modifier_string);
            modifier_string = tmp;
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
    }

    switch (ksym) {
        case XKB_KEY_u:
        case XKB_KEY_Escape:
            if ((ksym == XKB_KEY_u && ctrl) ||
                ksym == XKB_KEY_Escape) {
                DEBUG("C-u pressed\n");
                clear_input();
                /* Hide the unlock indicator after a bit if the password buffer is
                 * empty. */
                if (unlock_indicator) {
                    START_TIMER(clear_indicator_timeout, 1.0, clear_indicator_cb);
                    unlock_state = STATE_BACKSPACE_ACTIVE;
                    redraw_screen();
                    unlock_state = STATE_KEY_PRESSED;
                }
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

            if (input_position == 0)
                return;

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

    if ((input_position + 8) >= sizeof(password))
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
            break;
    }
}

/*
 * Called when the properties on the root window change, e.g. when the screen
 * resolution changes. If so we update the window to cover the whole screen
 * and also redraw the image, if any.
 *
 */
void handle_screen_resize(void) {
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

    xinerama_query_screens();
    redraw_screen();
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
        errx(EXIT_FAILURE, "X11 connection broke, did your server terminate?\n");

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
                        exit(0);

                    ev_loop_fork(EV_DEFAULT);
                }
                break;

            case XCB_CONFIGURE_NOTIFY:
                handle_screen_resize();
                break;

            default:
                if (type == xkb_base_event)
                    process_xkb_event(event);
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

    if ((conn = xcb_connect(NULL, &screens)) == NULL ||
        xcb_connection_has_error(conn))
        errx(EXIT_FAILURE, "Cannot open display\n");

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

int main(int argc, char *argv[]) {
    struct passwd *pw;
    char *username;
    char *image_path = NULL;
#ifndef __OpenBSD__
    int ret;
    struct pam_conv conv = {conv_callback, NULL};
#endif
    int curs_choice = CURS_NONE;
    int o;
    int optind = 0;
    struct option longopts[] = {
        {"version", no_argument, NULL, 'v'},
        {"nofork", no_argument, NULL, 'n'},
        {"beep", no_argument, NULL, 'b'},
        {"dpms", no_argument, NULL, 'd'},
        {"color", required_argument, NULL, 'c'},
        {"pointer", required_argument, NULL, 'p'},
        {"debug", no_argument, NULL, 0},
        {"help", no_argument, NULL, 'h'},
        {"no-unlock-indicator", no_argument, NULL, 'u'},
        {"image", required_argument, NULL, 'i'},
        {"tiling", no_argument, NULL, 't'},
        {"ignore-empty-password", no_argument, NULL, 'e'},
        {"inactivity-timeout", required_argument, NULL, 'I'},
        {"show-failed-attempts", no_argument, NULL, 'f'},
        /* options for unlock indicator colors */
        // defining a lot of different chars here for the options -- TODO find a nicer way for this, maybe not offering single character options at all
        {"insidevercolor", required_argument, NULL, 0},   // --i-v
        {"insidewrongcolor", required_argument, NULL, 0}, // --i-w
        {"insidecolor", required_argument, NULL, 0},      // --i-c
        {"ringvercolor", required_argument, NULL, 0},     // --r-v
        {"ringwrongcolor", required_argument, NULL, 0},   // --r-w
        {"ringcolor", required_argument, NULL, 0},        // --r-c
        {"linecolor", required_argument, NULL, 0},        // --l-c
        {"textcolor", required_argument, NULL, 0},        // --t-c
        {"timecolor", required_argument, NULL, 0},       
        {"datecolor", required_argument, NULL, 0},       
        {"keyhlcolor", required_argument, NULL, 0},       // --k-c
        {"bshlcolor", required_argument, NULL, 0},        // --b-c
        {"separatorcolor", required_argument, NULL, 0},
        {"line-uses-ring", no_argument, NULL, 'r'},
        {"line-uses-inside", no_argument, NULL, 's'},
        /* s for in_s_ide; ideally I'd use -I but that's used for timeout, which should use -T, but compatibility argh 
         * note: `I` has been deprecated for a while, so I might just remove that and reshuffle that? */
        {"screen", required_argument, NULL, 'S'},
        {"blur", required_argument, NULL, 'B'},
        {"clock", no_argument, NULL, 'k'},
        {"indicator", no_argument, NULL, 0},
        {"refresh-rate", required_argument, NULL, 0},
        {"composite", no_argument, NULL, 0},
        
        {"timestr", required_argument, NULL, 0},
        {"datestr", required_argument, NULL, 0},
        {"timefont", required_argument, NULL, 0},
        {"datefont", required_argument, NULL, 0},
        {"timesize", required_argument, NULL, 0},
        {"datesize", required_argument, NULL, 0},
        {"timepos", required_argument, NULL, 0},
        {"datepos", required_argument, NULL, 0},

        {"veriftext", required_argument, NULL, 0},
        {"wrongtext", required_argument, NULL, 0},
        {"textsize", required_argument, NULL, 0},
        {"modsize", required_argument, NULL, 0},
        {"radius", required_argument, NULL, 0},

        {NULL, no_argument, NULL, 0}};

    if ((pw = getpwuid(getuid())) == NULL)
        err(EXIT_FAILURE, "getpwuid() failed");
    if ((username = pw->pw_name) == NULL)
        errx(EXIT_FAILURE, "pw->pw_name is NULL.\n");

    char *optstring = "hvnbdc:p:ui:teI:frsS:kB:";
    while ((o = getopt_long(argc, argv, optstring, longopts, &optind)) != -1) {
        switch (o) {
            case 'v':
                errx(EXIT_SUCCESS, "version " VERSION " © 2010 Michael Stapelberg");
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
            case 'c': {
                char *arg = optarg;

                /* Skip # if present */
                if (arg[0] == '#')
                    arg++;

                if (strlen(arg) != 6 || sscanf(arg, "%06[0-9a-fA-F]", color) != 1)
                    errx(EXIT_FAILURE, "color is invalid, it must be given in 3-byte hexadecimal format: rrggbb\n");

                break;
            }
            case 'u':
                unlock_indicator = false;
                break;
            case 'i':
                image_path = strdup(optarg);
                break;
            case 't':
                tile = true;
                break;
            case 'p':
                if (!strcmp(optarg, "win")) {
                    curs_choice = CURS_WIN;
                } else if (!strcmp(optarg, "default")) {
                    curs_choice = CURS_DEFAULT;
                } else {
                    errx(EXIT_FAILURE, "i3lock: Invalid pointer type given. Expected one of \"win\" or \"default\".\n");
                }
                break;
            case 'e':
                ignore_empty_password = true;
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
            case 0:
                if (strcmp(longopts[optind].name, "debug") == 0)
                    debug_mode = true;
                else if (strcmp(longopts[optind].name, "indicator") == 0) {
                    show_indicator = true;
                }
                else if (strcmp(longopts[optind].name, "insidevercolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", insidevercolor) != 1)
                        errx(1, "insidevercolor is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "insidewrongcolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", insidewrongcolor) != 1)
                        errx(1, "insidewrongcolor is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "insidecolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", insidecolor) != 1)
                        errx(1, "insidecolor is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "ringvercolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", ringvercolor) != 1)
                        errx(1, "ringvercolor is invalid, color must be given in 4-byte format: rrggbb\n");
                }
                else if (strcmp(longopts[optind].name, "ringwrongcolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", ringwrongcolor) != 1)
                        errx(1, "ringwrongcolor is invalid, color must be given in r-byte format: rrggbb\n");
                }
                else if (strcmp(longopts[optind].name, "ringcolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", ringcolor) != 1)
                        errx(1, "ringcolor is invalid, color must be given in 4-byte format: rrggbb\n");
                }
                else if (strcmp(longopts[optind].name, "linecolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", linecolor) != 1)
                        errx(1, "linecolor is invalid, color must be given in 4-byte format: rrggbb\n");
                }
                else if (strcmp(longopts[optind].name, "textcolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", textcolor) != 1)
                        errx(1, "textcolor is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "timecolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", timecolor) != 1)
                        errx(1, "timecolor is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "datecolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", datecolor) != 1)
                        errx(1, "datecolor is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "keyhlcolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", keyhlcolor) != 1)
                        errx(1, "keyhlcolor is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "bshlcolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", bshlcolor) != 1)
                        errx(1, "bshlcolor is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "separatorcolor") == 0) {
                    char *arg = optarg;

                    /* Skip # if present */
                    if (arg[0] == '#')
                        arg++;

                    if (strlen(arg) != 8 || sscanf(arg, "%08[0-9a-fA-F]", separatorcolor) != 1)
                        errx(1, "separator is invalid, color must be given in 4-byte format: rrggbbaa\n");
                }
                else if (strcmp(longopts[optind].name, "timestr") == 0) {
                    //read in to timestr
                    if (strlen(optarg) > 31) {
                        errx(1, "time format string can be at most 31 characters\n");
                    }
                    strcpy(time_format,optarg);
                }
                else if (strcmp(longopts[optind].name, "datestr") == 0) {
                    //read in to datestr
                    if (strlen(optarg) > 31) {
                        errx(1, "time format string can be at most 31 characters\n");
                    }
                    strcpy(date_format,optarg);
                }
                else if (strcmp(longopts[optind].name, "timefont") == 0) {
                    //read in to time_font
                    if (strlen(optarg) > 31) {
                        errx(1, "time font string can be at most 31 characters\n");
                    }
                    strcpy(time_font,optarg);
                }
                else if (strcmp(longopts[optind].name, "datefont") == 0) {
                    //read in to date_font
                    if (strlen(optarg) > 31) {
                        errx(1, "date font string can be at most 31 characters\n");
                    }
                    strcpy(date_font,optarg);
                }
                else if (strcmp(longopts[optind].name, "timesize") == 0) {
                    char *arg = optarg;

                    if (sscanf(arg, "%lf", &time_size) != 1)
                        errx(1, "timesize must be a number\n");
                    if (time_size < 1)
                        errx(1, "timesize must be larger than 0\n");
                }
                else if (strcmp(longopts[optind].name, "datesize") == 0) {
                    char *arg = optarg;

                    if (sscanf(arg, "%lf", &date_size) != 1)
                        errx(1, "datesize must be a number\n");
                    if (date_size < 1)
                        errx(1, "datesize must be larger than 0\n");
                }
                else if (strcmp(longopts[optind].name, "timepos") == 0) {
                    //read in to time_x_expr and time_y_expr
                    if (strlen(optarg) > 31) {
                        // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                        errx(1, "time position string can be at most 31 characters\n");
                    }
                    char* arg = optarg;
                    if (sscanf(arg, "%30[^:]:%30[^:]", time_x_expr, time_y_expr) != 2) {
                        errx(1, "timepos must be of the form x:y\n");
                    }
                }
                else if (strcmp(longopts[optind].name, "datepos") == 0) {
                    //read in to date_x_expr and date_y_expr
                    if (strlen(optarg) > 31) {
                        // this is overly restrictive since both the x and y string buffers have size 32, but it's easier to check.
                        errx(1, "date position string can be at most 31 characters\n");
                    }
                    char* arg = optarg;
                    if (sscanf(arg, "%30[^:]:%30[^:]", date_x_expr, date_y_expr) != 2) {
                        errx(1, "datepos must be of the form x:y\n");
                    }
                }
                else if (strcmp(longopts[optind].name, "refresh-rate") == 0) {
                    char* arg = optarg;
                    refresh_rate = strtof(arg, NULL);
                    if (refresh_rate < 1.0) {
                        fprintf(stderr, "The given refresh rate of %fs is less than one second and was ignored.\n", refresh_rate);
                        refresh_rate = 1.0;
                    }
                }
                else if (strcmp(longopts[optind].name, "composite") == 0) {
                    composite = true;
                }
                else if (strcmp(longopts[optind].name, "veriftext") == 0) {
                    verif_text = optarg;
                }
                else if (strcmp(longopts[optind].name, "wrongtext") == 0) {
                    wrong_text = optarg;
                }
                else if (strcmp(longopts[optind].name, "textsize") == 0) {
                    char *arg = optarg;

                    if (sscanf(arg, "%lf", &text_size) != 1)
                        errx(1, "textsize must be a number\n");
                    if (text_size < 1) {
                        fprintf(stderr, "textsize must be a positive integer; ignoring...\n");
                        text_size = 28.0;
                    }
                }
                else if (strcmp(longopts[optind].name, "modsize") == 0) {
                    char *arg = optarg;

                    if (sscanf(arg, "%lf", &modifier_size) != 1)
                        errx(1, "modsize must be a number\n");
                    if (modifier_size < 1) {
                        fprintf(stderr, "modsize must be a positive integer; ignoring...\n");
                        modifier_size = 14.0;
                    }
                }
                else if (strcmp(longopts[optind].name, "radius") == 0) {
                    char *arg = optarg;

                    if (sscanf(arg, "%lf", &circle_radius) != 1)
                        errx(1, "radius must be a number\n");
                    if (circle_radius < 1) {
                        fprintf(stderr, "radius must be a positive integer; ignoring...\n");
                        circle_radius = 90.0;
                    }
                }
                break;
            case 'f':
                show_failed_attempts = true;
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

#if XKBCOMPOSE == 1
    const char *locale = getenv("LC_ALL");
    if (!locale)
        locale = getenv("LC_CTYPE");
    if (!locale)
        locale = getenv("LANG");
    if (!locale) {
        if (debug_mode)
            fprintf(stderr, "Can't detect your locale, fallback to C\n");
        locale = "C";
    }

    load_compose_table(locale);
#endif

    xinerama_init();
    xinerama_query_screens();

    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    last_resolution[0] = screen->width_in_pixels;
    last_resolution[1] = screen->height_in_pixels;

    xcb_change_window_attributes(conn, screen->root, XCB_CW_EVENT_MASK,
                                 (uint32_t[]){XCB_EVENT_MASK_STRUCTURE_NOTIFY});

    if (image_path) {
        /* Create a pixmap to render on, fill it with the background color */
        img = cairo_image_surface_create_from_png(image_path);
        /* In case loading failed, we just pretend no -i was specified. */
        if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
            fprintf(stderr, "Could not load image \"%s\": %s\n",
                    image_path, cairo_status_to_string(cairo_surface_status(img)));
            img = NULL;
        }
        free(image_path);
    }

    xcb_pixmap_t* blur_pixmap = NULL;
    if (blur) {
        blur_pixmap = malloc(sizeof(xcb_pixmap_t));
        xcb_visualtype_t *vistype = get_root_visual_type(screen);
        *blur_pixmap = capture_bg_pixmap(conn, screen, last_resolution);
        cairo_surface_t *xcb_img = cairo_xcb_surface_create(conn, *blur_pixmap, vistype, last_resolution[0], last_resolution[1]);

        blur_img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, last_resolution[0], last_resolution[1]);
        cairo_t *ctx = cairo_create(blur_img);
        cairo_set_source_surface(ctx, xcb_img, 0, 0);
        cairo_paint(ctx);

        cairo_destroy(ctx);
        cairo_surface_destroy(xcb_img);
        blur_image_surface(blur_img, blur_sigma);
    }

    /* Pixmap on which the image is rendered to (if any) */
    xcb_pixmap_t bg_pixmap = draw_image(last_resolution);

    /* Open the fullscreen window, already with the correct pixmap in place */
    win = open_fullscreen_window(conn, screen, color, bg_pixmap);
    xcb_free_pixmap(conn, bg_pixmap);
    if (blur_pixmap) {
        xcb_free_pixmap(conn, *blur_pixmap);
        free(blur_pixmap);
        blur_pixmap = NULL;
    }


    cursor = create_cursor(conn, screen, win, curs_choice);

    /* Display the "locking…" message while trying to grab the pointer/keyboard. */
    auth_state = STATE_AUTH_LOCK;
    grab_pointer_and_keyboard(conn, screen, cursor);

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
        errx(EXIT_FAILURE, "Could not initialize libev. Bad LIBEV_FLAGS?\n");

    /* Explicitly call the screen redraw in case "locking…" message was displayed */
    auth_state = STATE_AUTH_IDLE;
    redraw_screen();

    struct ev_io *xcb_watcher = calloc(sizeof(struct ev_io), 1);
    struct ev_check *xcb_check = calloc(sizeof(struct ev_check), 1);
    struct ev_prepare *xcb_prepare = calloc(sizeof(struct ev_prepare), 1);

    ev_io_init(xcb_watcher, xcb_got_event, xcb_get_file_descriptor(conn), EV_READ);
    ev_io_start(main_loop, xcb_watcher);

    ev_check_init(xcb_check, xcb_check_cb);
    ev_check_start(main_loop, xcb_check);

    ev_prepare_init(xcb_prepare, xcb_prepare_cb);
    ev_prepare_start(main_loop, xcb_prepare);

    /* Invoke the event callback once to catch all the events which were
     * received up until now. ev will only pick up new events (when the X11
     * file descriptor becomes readable). */
    ev_invoke(main_loop, xcb_check, 0);
    if (show_clock) {
        start_time_redraw_tick(main_loop);
    }
    ev_loop(main_loop, 0);
}
