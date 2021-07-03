/*
 * vim:ts=4:sw=4:expandtab
 *
 * © 2010 Michael Stapelberg
 * © 2021 Raymond Li
 *
 * See LICENSE for licensing information
 *
 */
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <xcb/xcb.h>
#include <xcb/xinerama.h>
#include <xcb/randr.h>

#include "i3lock.h"
#include "xcb.h"
#include "randr.h"

/* Number of Xinerama screens which are currently present. */
int xr_screens = 0;

/* The resolutions of the currently present Xinerama screens. */
Rect *xr_resolutions = NULL;

static bool xinerama_active;
static bool has_randr = false;
static bool has_randr_1_5 = false;
extern bool debug_mode;

void _xinerama_init(void);

void randr_init(int *event_base, xcb_window_t root) {
    const xcb_query_extension_reply_t *extreply;

    extreply = xcb_get_extension_data(conn, &xcb_randr_id);
    if (!extreply->present) {
        DEBUG("RandR is not present, falling back to Xinerama.\n");
        _xinerama_init();
        return;
    }

    xcb_generic_error_t *err;
    xcb_randr_query_version_reply_t *randr_version =
        xcb_randr_query_version_reply(
            conn, xcb_randr_query_version(conn, XCB_RANDR_MAJOR_VERSION, XCB_RANDR_MINOR_VERSION), &err);
    if (err != NULL) {
        DEBUG("Could not query RandR version: X11 error code %d\n", err->error_code);
        _xinerama_init();
        return;
    }

    has_randr = true;

    has_randr_1_5 = (randr_version->major_version >= 1) &&
                    (randr_version->minor_version >= 5);

    free(randr_version);

    if (event_base != NULL)
        *event_base = extreply->first_event;

    xcb_randr_select_input(conn, root,
                           XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
                               XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
                               XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
                               XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);

    xcb_flush(conn);
}

void _xinerama_init(void) {
    if (!xcb_get_extension_data(conn, &xcb_xinerama_id)->present) {
        DEBUG("Xinerama extension not found, disabling.\n");
        return;
    }

    xcb_xinerama_is_active_cookie_t cookie;
    xcb_xinerama_is_active_reply_t *reply;

    cookie = xcb_xinerama_is_active(conn);
    reply = xcb_xinerama_is_active_reply(conn, cookie, NULL);
    if (!reply)
        return;

    if (!reply->state) {
        free(reply);
        return;
    }

    xinerama_active = true;
    free(reply);
}

/*
 * randr_query_outputs_15 uses RandR ≥ 1.5 to update outputs.
 *
 */
static bool _randr_query_monitors_15(xcb_window_t root) {
#if XCB_RANDR_MINOR_VERSION < 5
    return false;
#else
    /* RandR 1.5 available at compile-time, i.e. libxcb is new enough */
    if (!has_randr_1_5) {
        return false;
    }
    /* RandR 1.5 available at run-time (supported by the server) */
    DEBUG("Querying monitors using RandR 1.5\n");
    xcb_generic_error_t *err;
    xcb_randr_get_monitors_reply_t *monitors =
        xcb_randr_get_monitors_reply(
            conn, xcb_randr_get_monitors(conn, root, true), &err);
    if (err != NULL) {
        DEBUG("Could not get RandR monitors: X11 error code %d\n", err->error_code);
        free(err);
        /* Fall back to RandR ≤ 1.4 */
        return false;
    }

    int screens = xcb_randr_get_monitors_monitors_length(monitors);
    DEBUG("%d RandR monitors found (timestamp %d)\n",
          screens, monitors->timestamp);

    Rect *resolutions = malloc(screens * sizeof(Rect));
    /* No memory? Just keep on using the old information. */
    if (!resolutions) {
        free(monitors);
        return true;
    }

    xcb_randr_monitor_info_iterator_t iter;
    int screen;
    for (iter = xcb_randr_get_monitors_monitors_iterator(monitors), screen = 0;
         iter.rem;
         xcb_randr_monitor_info_next(&iter), screen++) {
        const xcb_randr_monitor_info_t *monitor_info = iter.data;

        resolutions[screen].x = monitor_info->x;
        resolutions[screen].y = monitor_info->y;
        resolutions[screen].width = monitor_info->width;
        resolutions[screen].height = monitor_info->height;
        DEBUG("found RandR monitor: %d x %d at %d x %d\n",
              monitor_info->width, monitor_info->height,
              monitor_info->x, monitor_info->y);
    }
    free(xr_resolutions);
    xr_resolutions = resolutions;
    xr_screens = screens;

    free(monitors);
    return true;
#endif
}

/*
 * randr_query_outputs_14 uses RandR ≤ 1.4 to update outputs.
 *
 */
static bool _randr_query_outputs_14(xcb_window_t root) {
    if (!has_randr) {
        return false;
    }
    DEBUG("Querying outputs using RandR ≤ 1.4\n");

    /* Get screen resources (primary output, crtcs, outputs, modes) */
    xcb_randr_get_screen_resources_current_cookie_t rcookie;
    rcookie = xcb_randr_get_screen_resources_current(conn, root);

    xcb_randr_get_screen_resources_current_reply_t *res =
        xcb_randr_get_screen_resources_current_reply(conn, rcookie, NULL);
    if (res == NULL) {
        DEBUG("Could not query screen resources.\n");
        return false;
    }

    /* timestamp of the configuration so that we get consistent replies to all
     * requests (if the configuration changes between our different calls) */
    const xcb_timestamp_t cts = res->config_timestamp;

    const int len = xcb_randr_get_screen_resources_current_outputs_length(res);

    /* an output is VGA-1, LVDS-1, etc. (usually physical video outputs) */
    xcb_randr_output_t *randr_outputs = xcb_randr_get_screen_resources_current_outputs(res);

    /* Request information for each output */
    xcb_randr_get_output_info_cookie_t ocookie[len];
    for (int i = 0; i < len; i++) {
        ocookie[i] = xcb_randr_get_output_info(conn, randr_outputs[i], cts);
    }
    Rect *resolutions = malloc(len * sizeof(Rect));
    /* No memory? Just keep on using the old information. */
    if (!resolutions) {
        free(res);
        return true;
    }

    /* Loop through all outputs available for this X11 screen */
    int screen = 0;

    for (int i = 0; i < len; i++) {
        xcb_randr_get_output_info_reply_t *output;

        if ((output = xcb_randr_get_output_info_reply(conn, ocookie[i], NULL)) == NULL) {
            continue;
        }

        if (output->crtc == XCB_NONE) {
            free(output);
            continue;
        }

        xcb_randr_get_crtc_info_cookie_t icookie;
        xcb_randr_get_crtc_info_reply_t *crtc;
        icookie = xcb_randr_get_crtc_info(conn, output->crtc, cts);
        if ((crtc = xcb_randr_get_crtc_info_reply(conn, icookie, NULL)) == NULL) {
            DEBUG("Skipping output: could not get CRTC (0x%08x)\n", output->crtc);
            free(output);
            continue;
        }

        resolutions[screen].x = crtc->x;
        resolutions[screen].y = crtc->y;
        resolutions[screen].width = crtc->width;
        resolutions[screen].height = crtc->height;

        DEBUG("found RandR output: %d x %d at %d x %d\n",
              crtc->width, crtc->height,
              crtc->x, crtc->y);

        screen++;

        free(crtc);

        free(output);
    }
    free(xr_resolutions);
    xr_resolutions = resolutions;
    xr_screens = screen;
    free(res);
    return true;
}

void _xinerama_query_screens(void) {
    if (!xinerama_active) {
        return;
    }

    xcb_xinerama_query_screens_cookie_t cookie;
    xcb_xinerama_query_screens_reply_t *reply;
    xcb_xinerama_screen_info_t *screen_info;
    xcb_generic_error_t *err;
    cookie = xcb_xinerama_query_screens_unchecked(conn);
    reply = xcb_xinerama_query_screens_reply(conn, cookie, &err);
    if (!reply) {
        DEBUG("Couldn't get Xinerama screens: X11 error code %d\n", err->error_code);
        free(err);
        return;
    }
    screen_info = xcb_xinerama_query_screens_screen_info(reply);
    int screens = xcb_xinerama_query_screens_screen_info_length(reply);

    Rect *resolutions = malloc(screens * sizeof(Rect));
    /* No memory? Just keep on using the old information. */
    if (!resolutions) {
        free(reply);
        return;
    }

    for (int screen = 0; screen < xr_screens; screen++) {
        resolutions[screen].x = screen_info[screen].x_org;
        resolutions[screen].y = screen_info[screen].y_org;
        resolutions[screen].width = screen_info[screen].width;
        resolutions[screen].height = screen_info[screen].height;
        DEBUG("found Xinerama screen: %d x %d at %d x %d\n",
              screen_info[screen].width, screen_info[screen].height,
              screen_info[screen].x_org, screen_info[screen].y_org);
    }

    free(xr_resolutions);
    xr_resolutions = resolutions;
    xr_screens = screens;

    free(reply);
}

void randr_query(xcb_window_t root) {
    if (_randr_query_monitors_15(root)) {
        return;
    }

    if (_randr_query_outputs_14(root)) {
        return;
    }

    _xinerama_query_screens();
}
