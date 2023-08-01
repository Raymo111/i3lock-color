#ifndef _XCB_H
#define _XCB_H

#include <xcb/xcb.h>

#define all_name_details                                 \
    (XCB_XKB_NAME_DETAIL_KEYCODES |                      \
     XCB_XKB_NAME_DETAIL_GEOMETRY |                      \
     XCB_XKB_NAME_DETAIL_SYMBOLS |                       \
     XCB_XKB_NAME_DETAIL_PHYS_SYMBOLS |                  \
     XCB_XKB_NAME_DETAIL_TYPES |                         \
     XCB_XKB_NAME_DETAIL_COMPAT |                        \
     XCB_XKB_NAME_DETAIL_KEY_TYPE_NAMES |                \
     XCB_XKB_NAME_DETAIL_KT_LEVEL_NAMES |                \
     XCB_XKB_NAME_DETAIL_INDICATOR_NAMES |               \
     XCB_XKB_NAME_DETAIL_KEY_NAMES |                     \
     XCB_XKB_NAME_DETAIL_KEY_ALIASES |                   \
     XCB_XKB_NAME_DETAIL_VIRTUAL_MOD_NAMES |             \
     XCB_XKB_NAME_DETAIL_GROUP_NAMES |                   \
     XCB_XKB_NAME_DETAIL_RG_NAMES)


extern xcb_connection_t *conn;
extern xcb_screen_t *screen;

xcb_visualtype_t *get_root_visual_type(xcb_screen_t *s);
xcb_visualtype_t* get_visualtype_by_depth(uint16_t depth, xcb_screen_t* root_screen);
xcb_pixmap_t create_bg_pixmap(xcb_connection_t *conn, xcb_drawable_t drawable, u_int32_t *resolution, char *color);
xcb_window_t open_fullscreen_window(xcb_connection_t *conn, xcb_screen_t *scr, char *color);
bool grab_pointer_and_keyboard(xcb_connection_t *conn, xcb_screen_t *screen, xcb_cursor_t cursor, int tries);
xcb_cursor_t create_cursor(xcb_connection_t *conn, xcb_screen_t *screen, xcb_window_t win, int choice);
xcb_window_t find_focused_window(xcb_connection_t *conn, const xcb_window_t root);
void set_focused_window(xcb_connection_t *conn, const xcb_window_t root, const xcb_window_t window);
xcb_pixmap_t capture_bg_pixmap(xcb_connection_t *conn, xcb_screen_t *scr, u_int32_t* resolution);
char* xcb_get_key_group_names(xcb_connection_t *conn);

#endif
