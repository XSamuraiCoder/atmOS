#ifndef SHELL_SETTINGS_H
#define SHELL_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

/* Built-in Settings app. Same hook shape as Files / Notes in the app_t table. */

typedef struct { uint8_t r, g, b; } settings_rgb_t;

typedef struct {
    settings_rgb_t top, bottom;     /* wallpaper gradient */
    settings_rgb_t bar, bar_line;   /* menu bar */
    settings_rgb_t dock;
    settings_rgb_t accent;
} settings_theme_t;

/* values used by the shell */
void settings_init(void);                       /* load saved settings; call once per start */
const settings_theme_t *settings_theme(void);   /* current colors */
uint8_t settings_pointer_speed(void);           /* 0 = slow, 1 = normal, 2 = fast */
bool settings_clock_24h(void);                  /* false = 12-hour with AM/PM */
bool settings_take_changed(void);               /* true once after a change: re-apply the palette */

/* app hooks */
void settings_open(void);                       /* call when the window opens */
void settings_draw(int x, int y);
void settings_draw_region(int x, int y, int x0, int y0, int x1, int y1);   /* only what touches the region */
void settings_click(int x, int y);
bool settings_hover(int x, int y);
/* Screen rectangles (x0, y0, x1, y1; x1/y1 exclusive) that changed since the last call, so the
 * shell repaints only those. Returns the count, or -1 when the whole window should be repainted. */
int settings_dirty(int x, int y, int r[][4], int max);
bool settings_keys(void);                       /* always false: arrows move the pointer */
bool settings_press(void);                      /* always false: Enter clicks */
void settings_flush(void);                      /* archive the AppVar; call before leaving the shell */

#endif