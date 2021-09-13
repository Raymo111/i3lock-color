/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2010 Michael Stapelberg
 * © 2021 Raymond Li
 *
 * xcb.c: contains all functions which use XCB to talk to X11. Mostly wrappers
 *        around the rather complicated/ugly parts of the XCB API.
 *
 */
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>
#include <xcb/composite.h>
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <err.h>
#include <time.h>
#include <sys/time.h>

#include "cursors.h"
#include "i3lock.h"
#include "xcb.h"
#include "unlock_indicator.h"

extern auth_state_t auth_state;
extern bool composite;
extern bool debug_mode;

xcb_connection_t *conn;
xcb_screen_t *screen;

static xcb_atom_t _NET_WM_BYPASS_COMPOSITOR = XCB_NONE;
void _init_net_wm_bypass_compositor(xcb_connection_t *conn) {
    if (_NET_WM_BYPASS_COMPOSITOR != XCB_NONE) {
        /* already initialized */
        return;
    }
    xcb_generic_error_t *err;
    xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(
        conn,
        xcb_intern_atom(conn, 0, strlen("_NET_WM_BYPASS_COMPOSITOR"), "_NET_WM_BYPASS_COMPOSITOR"),
        &err);
    if (atom_reply == NULL) {
        fprintf(stderr, "X11 Error %d\n", err->error_code);
        free(err);
        return;
    }
    _NET_WM_BYPASS_COMPOSITOR = atom_reply->atom;
    free(atom_reply);
}

#define curs_invisible_width 8
#define curs_invisible_height 8

static unsigned char curs_invisible_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define curs_windows_width 11
#define curs_windows_height 19

static unsigned char curs_windows_bits[] = {
    0xfe, 0x07, 0xfc, 0x07, 0xfa, 0x07, 0xf6, 0x07, 0xee, 0x07, 0xde, 0x07,
    0xbe, 0x07, 0x7e, 0x07, 0xfe, 0x06, 0xfe, 0x05, 0x3e, 0x00, 0xb6, 0x07,
    0x6a, 0x07, 0x6c, 0x07, 0xde, 0x06, 0xdf, 0x06, 0xbf, 0x05, 0xbf, 0x05,
    0x7f, 0x06};

#define mask_windows_width 11
#define mask_windows_height 19

static unsigned char mask_windows_bits[] = {
    0x01, 0x00, 0x03, 0x00, 0x07, 0x00, 0x0f, 0x00, 0x1f, 0x00, 0x3f, 0x00,
    0x7f, 0x00, 0xff, 0x00, 0xff, 0x01, 0xff, 0x03, 0xff, 0x07, 0x7f, 0x00,
    0xf7, 0x00, 0xf3, 0x00, 0xe1, 0x01, 0xe0, 0x01, 0xc0, 0x03, 0xc0, 0x03,
    0x80, 0x01};

static uint32_t get_colorpixel(char *hex) {
    char strgroups[4][3] = {{hex[0], hex[1], '\0'},
                            {hex[2], hex[3], '\0'},
                            {hex[4], hex[5], '\0'},
                            {hex[6], hex[4], '\0'}};
    uint32_t rgb16[4] = {(strtol(strgroups[0], NULL, 16)),
                         (strtol(strgroups[1], NULL, 16)),
                         (strtol(strgroups[2], NULL, 16)),
                         (strtol(strgroups[3], NULL, 16))};

    return (rgb16[3] << 24) + (rgb16[0] << 16) + (rgb16[1] << 8) + rgb16[2];
}

xcb_visualtype_t *get_visualtype_by_depth(uint16_t depth, xcb_screen_t *root_screen) {
    xcb_depth_iterator_t depth_iter;

    depth_iter = xcb_screen_allowed_depths_iterator(root_screen);
    for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
        if (depth_iter.data->depth != depth)
            continue;

        xcb_visualtype_iterator_t visual_iter;

        visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
        if (!visual_iter.rem)
            continue;
        return visual_iter.data;
    }
    return NULL;
}


xcb_visualtype_t *get_root_visual_type(xcb_screen_t *screen) {
    xcb_visualtype_t *visual_type = NULL;
    xcb_depth_iterator_t depth_iter;
    xcb_visualtype_iterator_t visual_iter;

    for (depth_iter = xcb_screen_allowed_depths_iterator(screen);
         depth_iter.rem;
         xcb_depth_next(&depth_iter)) {
        for (visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
             visual_iter.rem;
             xcb_visualtype_next(&visual_iter)) {
            if (screen->root_visual != visual_iter.data->visual_id)
                continue;

            visual_type = visual_iter.data;
            return visual_type;
        }
    }

    return NULL;
}

xcb_pixmap_t create_bg_pixmap(xcb_connection_t *conn, xcb_drawable_t win, u_int32_t *resolution, char *color) {
    xcb_pixmap_t bg_pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, 32, bg_pixmap, win, resolution[0], resolution[1]);

    /* Generate a Graphics Context and fill the pixmap with background color
     * (for images that are smaller than your screen) */
    xcb_gcontext_t gc = xcb_generate_id(conn);
    uint32_t values[] = {get_colorpixel(color)};
    xcb_create_gc(conn, gc, bg_pixmap, XCB_GC_FOREGROUND, values);
    xcb_rectangle_t rect = {0, 0, resolution[0], resolution[1]};
    xcb_poly_fill_rectangle(conn, bg_pixmap, gc, 1, &rect);
    xcb_free_gc(conn, gc);

    return bg_pixmap;
}

xcb_window_t open_fullscreen_window(xcb_connection_t *conn, xcb_screen_t *scr, char *color) {
    uint32_t mask = 0;
    uint32_t values[5];
    xcb_window_t win = xcb_generate_id(conn);
    xcb_window_t parent_win = scr->root;

    if (composite) {
        /* Check whether the composite extension is available */
        const xcb_query_extension_reply_t *extension_query = NULL;
        xcb_generic_error_t *error = NULL;
        xcb_composite_get_overlay_window_cookie_t cookie;
        xcb_composite_get_overlay_window_reply_t *composite_reply = NULL;

        extension_query = xcb_get_extension_data(conn, &xcb_composite_id);
        if (extension_query && extension_query->present) {
            /* When composition is used, we need to use the composite overlay
             * window instead of the normal root window to be able to cover
             * composited windows */
            cookie = xcb_composite_get_overlay_window(conn, scr->root);
            composite_reply = xcb_composite_get_overlay_window_reply(conn, cookie, &error);

            if (!error && composite_reply) {
                parent_win = composite_reply->overlay_win;
            }

            free(composite_reply);
            free(error);
        }
    }


    xcb_visualid_t visual = get_visualtype_by_depth(32, scr)->visual_id;
    xcb_colormap_t win_colormap = xcb_generate_id(conn);
    xcb_create_colormap(conn, XCB_COLORMAP_ALLOC_NONE, win_colormap, scr->root, visual);

    mask |= XCB_CW_BACK_PIXEL;
    values[0] = get_colorpixel(color);

    mask |= XCB_CW_BORDER_PIXEL;
    values[1] = 0x00000000;

    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[2] = 1;

    mask |= XCB_CW_EVENT_MASK;
    values[3] = XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_KEY_PRESS |
                XCB_EVENT_MASK_KEY_RELEASE |
                XCB_EVENT_MASK_VISIBILITY_CHANGE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    mask |= XCB_CW_COLORMAP;
    values[4] = win_colormap;

    xcb_create_window(conn,
                      32,
                      win, /* the window id */
                      parent_win,
                      0, 0,
                      scr->width_in_pixels,
                      scr->height_in_pixels, /* dimensions */
                      0,                     /* border = 0, we draw our own */
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      visual, /* copy visual from parent */
                      mask,
                      values);

    char *name = "i3lock";
    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        win,
                        XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING,
                        8,
                        strlen(name),
                        name);

    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        win,
                        XCB_ATOM_WM_CLASS,
                        XCB_ATOM_STRING,
                        8,
                        2 * (strlen("i3lock") + 1),
                        "i3lock\0i3lock\0");

    const uint32_t bypass_compositor = 1; /* disable compositing */
    _init_net_wm_bypass_compositor(conn);
    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        win,
                        _NET_WM_BYPASS_COMPOSITOR,
                        XCB_ATOM_CARDINAL,
                        32,
                        1,
                        &bypass_compositor);

    /* Map the window (= make it visible) */
    xcb_map_window(conn, win);

    /* Raise window (put it on top) */
    values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, values);

    /* Ensure that the window is created and set up before returning */
    xcb_aux_sync(conn);

    return win;
}

/*
 * Repeatedly tries to grab pointer and keyboard (up to the specified number of
 * tries).
 *
 * Returns true if the grab succeeded, false if not.
 *
 */
bool grab_pointer_and_keyboard(xcb_connection_t *conn, xcb_screen_t *screen, xcb_cursor_t cursor, int tries) {
    xcb_grab_pointer_cookie_t pcookie;
    xcb_grab_pointer_reply_t *preply;

    xcb_grab_keyboard_cookie_t kcookie;
    xcb_grab_keyboard_reply_t *kreply;

    const suseconds_t screen_redraw_timeout = 100000; /* 100ms */

    /* Using few variables to trigger a redraw_screen() if too many tries */
    bool redrawn = false;
    struct timeval start;
    if (gettimeofday(&start, NULL) == -1) {
        err(EXIT_FAILURE, "gettimeofday");
    }

    while (tries-- > 0) {
        pcookie = xcb_grab_pointer(
            conn,
            false,               /* get all pointer events specified by the following mask */
            screen->root,        /* grab the root window */
            XCB_NONE,            /* which events to let through */
            XCB_GRAB_MODE_ASYNC, /* pointer events should continue as normal */
            XCB_GRAB_MODE_ASYNC, /* keyboard mode */
            XCB_NONE,            /* confine_to = in which window should the cursor stay */
            cursor,              /* we change the cursor to whatever the user wanted */
            XCB_CURRENT_TIME);

        if ((preply = xcb_grab_pointer_reply(conn, pcookie, NULL)) &&
            preply->status == XCB_GRAB_STATUS_SUCCESS) {
            free(preply);
            break;
        }

        /* In case the grab failed, we still need to free the reply */
        free(preply);

        /* Make this quite a bit slower */
        usleep(50);

        struct timeval now;
        if (gettimeofday(&now, NULL) == -1) {
            err(EXIT_FAILURE, "gettimeofday");
        }

        struct timeval elapsed;
        timersub(&now, &start, &elapsed);

        if (!redrawn &&
            (tries % 100) == 0 &&
            elapsed.tv_usec >= screen_redraw_timeout) {
            redraw_screen();
            redrawn = true;
        }
    }

    while (tries-- > 0) {
        kcookie = xcb_grab_keyboard(
            conn,
            true,         /* report events */
            screen->root, /* grab the root window */
            XCB_CURRENT_TIME,
            XCB_GRAB_MODE_ASYNC, /* process events as normal, do not require sync */
            XCB_GRAB_MODE_ASYNC);

        if ((kreply = xcb_grab_keyboard_reply(conn, kcookie, NULL)) &&
            kreply->status == XCB_GRAB_STATUS_SUCCESS) {
            free(kreply);
            break;
        }

        /* In case the grab failed, we still need to free the reply */
        free(kreply);

        /* Make this quite a bit slower */
        usleep(50);

        struct timeval now;
        if (gettimeofday(&now, NULL) == -1) {
            err(EXIT_FAILURE, "gettimeofday");
        }

        struct timeval elapsed;
        timersub(&now, &start, &elapsed);

        /* Trigger a screen redraw if 100ms elapsed */
        if (!redrawn &&
            (tries % 100) == 0 &&
            elapsed.tv_usec >= screen_redraw_timeout) {
            redraw_screen();
            redrawn = true;
        }
    }

    return (tries > 0);
}

xcb_cursor_t create_cursor(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win, int choice) {
    xcb_pixmap_t bitmap;
    xcb_pixmap_t mask;
    xcb_cursor_t cursor;

    unsigned char *curs_bits;
    unsigned char *mask_bits;
    int curs_w, curs_h;

    switch (choice) {
        case CURS_NONE:
            curs_bits = curs_invisible_bits;
            mask_bits = curs_invisible_bits;
            curs_w = curs_invisible_width;
            curs_h = curs_invisible_height;
            break;
        case CURS_WIN:
            curs_bits = curs_windows_bits;
            mask_bits = mask_windows_bits;
            curs_w = curs_windows_width;
            curs_h = curs_windows_height;
            break;
        case CURS_DEFAULT:
        default:
            return XCB_NONE; /* XCB_NONE is xcb's way of saying "don't change the cursor" */
    }

    bitmap = xcb_create_pixmap_from_bitmap_data(conn,
                                                win,
                                                curs_bits,
                                                curs_w,
                                                curs_h,
                                                1,
                                                screen->white_pixel,
                                                screen->black_pixel,
                                                NULL);

    mask = xcb_create_pixmap_from_bitmap_data(conn,
                                              win,
                                              mask_bits,
                                              curs_w,
                                              curs_h,
                                              1,
                                              screen->white_pixel,
                                              screen->black_pixel,
                                              NULL);

    cursor = xcb_generate_id(conn);

    xcb_create_cursor(conn,
                      cursor,
                      bitmap,
                      mask,
                      65535, 65535, 65535,
                      0, 0, 0,
                      0, 0);

    xcb_free_pixmap(conn, bitmap);
    xcb_free_pixmap(conn, mask);

    return cursor;
}

static xcb_atom_t _NET_ACTIVE_WINDOW = XCB_NONE;
void _init_net_active_window(xcb_connection_t *conn) {
    if (_NET_ACTIVE_WINDOW != XCB_NONE) {
        /* already initialized */
        return;
    }
    xcb_generic_error_t *err;
    xcb_intern_atom_reply_t *atom_reply = xcb_intern_atom_reply(
        conn,
        xcb_intern_atom(conn, 0, strlen("_NET_ACTIVE_WINDOW"), "_NET_ACTIVE_WINDOW"),
        &err);
    if (atom_reply == NULL) {
        fprintf(stderr, "X11 Error %d\n", err->error_code);
        free(err);
        return;
    }
    _NET_ACTIVE_WINDOW = atom_reply->atom;
    free(atom_reply);
}

xcb_window_t find_focused_window(xcb_connection_t *conn, const xcb_window_t root) {
    xcb_window_t result = XCB_NONE;

    _init_net_active_window(conn);

    xcb_get_property_reply_t *prop_reply = xcb_get_property_reply(
        conn,
        xcb_get_property_unchecked(
            conn, false, root, _NET_ACTIVE_WINDOW, XCB_GET_PROPERTY_TYPE_ANY, 0, 1 /* word */),
        NULL);
    if (prop_reply == NULL) {
        goto out;
    }
    if (xcb_get_property_value_length(prop_reply) == 0) {
        goto out_prop;
    }
    if (prop_reply->type != XCB_ATOM_WINDOW) {
        goto out_prop;
    }

    result = *((xcb_window_t *)xcb_get_property_value(prop_reply));

out_prop:
    free(prop_reply);
out:
    return result;
}

void set_focused_window(xcb_connection_t *conn, const xcb_window_t root, const xcb_window_t window) {
    xcb_client_message_event_t ev;
    memset(&ev, '\0', sizeof(xcb_client_message_event_t));

    _init_net_active_window(conn);

    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = window;
    ev.type = _NET_ACTIVE_WINDOW;
    ev.format = 32;
    ev.data.data32[0] = 2; /* 2 = pager */

    xcb_send_event(conn, false, root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (char *)&ev);
    xcb_flush(conn);
}

xcb_pixmap_t capture_bg_pixmap(xcb_connection_t *conn, xcb_screen_t *scr, u_int32_t * resolution) {
    xcb_pixmap_t bg_pixmap = xcb_generate_id(conn);
    xcb_create_pixmap(conn, scr->root_depth, bg_pixmap, scr->root, resolution[0], resolution[1]);
    xcb_gcontext_t gc = xcb_generate_id(conn);
    uint32_t values[] = { scr->black_pixel, 1};
    xcb_create_gc(conn, gc, bg_pixmap, XCB_GC_FOREGROUND | XCB_GC_SUBWINDOW_MODE, values);
    xcb_rectangle_t rect = { 0, 0, resolution[0], resolution[1] };
    xcb_poly_fill_rectangle(conn, bg_pixmap, gc, 1, &rect);
    xcb_copy_area(conn, scr->root, bg_pixmap, gc, 0, 0, 0, 0, resolution[0], resolution[1]);
    xcb_flush(conn);
    xcb_free_gc(conn, gc);
    return bg_pixmap;
}

static char * get_atom_name(xcb_connection_t* conn, xcb_atom_t atom) {
    xcb_get_atom_name_reply_t *reply = NULL;
    char *name;
    int length;
    char* answer = NULL;

    if (atom == 0)
        return "<empty>";

		xcb_get_atom_name_cookie_t cookie;
		xcb_generic_error_t *error = NULL;

		cookie = xcb_get_atom_name(conn, atom);

		reply = xcb_get_atom_name_reply(conn, cookie, &error);
		if (!reply || error)
				return "<invalid>";

    length = xcb_get_atom_name_name_length(reply);
    name = xcb_get_atom_name_name(reply);

    answer = malloc(sizeof(char) * (length + 1));
    strncpy(answer, name, length);
    answer[length] = '\0';
    free(error);
    free(reply);
    return answer;
}


uint8_t xcb_get_key_group_index(xcb_connection_t *conn) {
    xcb_xkb_get_state_cookie_t cookie;
    xcb_xkb_get_state_reply_t* reply = NULL;
    cookie = xcb_xkb_get_state(conn, XCB_XKB_ID_USE_CORE_KBD);
    reply = xcb_xkb_get_state_reply(conn, cookie, NULL);
    uint8_t ans = reply->group;
    free(reply);
    return ans;
}


char* xcb_get_key_group_names(xcb_connection_t *conn) {
    uint8_t xkb_base_event;
    uint8_t xkb_base_error;
    if (xkb_x11_setup_xkb_extension(conn,
                                    XKB_X11_MIN_MAJOR_XKB_VERSION,
                                    XKB_X11_MIN_MINOR_XKB_VERSION,
                                    0,
                                    NULL,
                                    NULL,
                                    &xkb_base_event,
                                    &xkb_base_error) != 1)
    errx(EXIT_FAILURE, "Could not setup XKB extension.");


    xcb_xkb_get_names_reply_t *reply = NULL;


    xcb_generic_error_t *error = NULL;
    xcb_xkb_get_names_cookie_t cookie;

    cookie = xcb_xkb_get_names(conn,
                                 XCB_XKB_ID_USE_CORE_KBD,
                                 all_name_details);

    reply = xcb_xkb_get_names_reply(conn, cookie, &error);
    if (!reply || error)
            errx(1, "couldn't get reply for get_names");

    xcb_xkb_get_names_value_list_t list;

    void *buffer;

    buffer = xcb_xkb_get_names_value_list(reply);
    xcb_xkb_get_names_value_list_unpack(buffer,
                                        reply->nTypes,
                                        reply->indicators,
                                        reply->virtualMods,
                                        reply->groupNames,
                                        reply->nKeys,
                                        reply->nKeyAliases,
                                        reply->nRadioGroups,
                                        reply->which,
                                        &list);

    /* dump group names. */

    int length;
    xcb_atom_t *iter;
    uint8_t index;
    char* answer = NULL;
    length = xcb_xkb_get_names_value_list_groups_length(reply, &list);
    iter = xcb_xkb_get_names_value_list_groups(&list);
    index = xcb_get_key_group_index(conn);

    for (int i = 0; i < length; i++) {
            xcb_atom_t group_name = *iter;
            char* name = get_atom_name(conn, group_name);
            DEBUG("group_name %d: %s\n", i, name);
            if (i == index) {
                answer = name;
            } else {
                free(name);
            }
            iter++;
    }
    free(reply);
    free(error);
    return answer;
}
