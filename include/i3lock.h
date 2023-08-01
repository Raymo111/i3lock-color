#ifndef _I3LOCK_H
#define _I3LOCK_H

// boy i sure hope this doesnt change in the future
#define NANOSECONDS_IN_SECOND 1000000000

/* This macro will only print debug output when started with --debug.
 * This is important because xautolock (for example) closes stdout/stderr by
 * default, so just printing something to stdout will lead to the data ending
 * up on the X11 socket (!). */
 #define DEBUG(fmt, ...)                                            \
     do {                                                           \
         if (debug_mode) {                                          \
             fprintf(stderr, "[i3lock-debug] " fmt, ##__VA_ARGS__); \
         }                                                          \
    } while (0)

#endif
