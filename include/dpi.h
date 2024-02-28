#pragma once

/**
 * Initialize the DPI setting.
 * This will use the 'Xft.dpi' X resource if available and fall back to
 * guessing the correct value otherwise.
 */
void init_dpi(void);

/**
 * This function returns the value of the DPI setting.
 *
 */
long get_dpi_value(void);

/**
 * Convert a logical amount of pixels (e.g. 2 pixels on a “standard” 96 DPI
 * screen) to a corresponding amount of physical pixels on a standard or retina
 * screen, e.g. 5 pixels on a 227 DPI MacBook Pro 13" Retina screen.
 *
 */
int logical_px(const int logical);
