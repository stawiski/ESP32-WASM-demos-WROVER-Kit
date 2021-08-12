#ifndef __UTILS_H_
#define __UTILS_H_

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Generate random LV color.
///
/// @return Randomized color.
lv_color_t makeRandomColor(void);

/// Get random float in range [0;1].
///
/// @return Random float number.
float randf(void);

/// Get current time in microseconds.
///
/// Arduino-like API.
///
/// @return Current time in microseconds.
uint64_t micros(void);

/// Get current time in milliseconds.
///
/// Arduino-like API.
///
/// @return Current time in milliseconds.
uint64_t millis(void);

#ifdef __cplusplus
}
#endif

#endif // __UTILS_H_