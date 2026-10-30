#ifndef SHELL_SETTINGS_H
#define SHELL_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

/* Built-in Settings app (tabs: Look / Taskbar / System, dropdown menus).
 * Same hook shape as Files / Notes in the app_t table. */

typedef struct { uint8_t r, g, b; } settings_rgb_t;

typedef struct {
    settings_rgb_t top, bottom;     /* wallpaper gradient */
    settings_rgb_t bar, bar_line;   /* menu bar */
    settings_rgb_t dock;
    settings_rgb_t accent;
} settings_theme_t;

/* values used by the shell */
void settings_init(void);                       /* load saved settings; call once per start */
const settings_theme_t *settings_theme(void);   /* effective colors (preset + taskbar/accent/wallpaper choices) */

/* Runtime theme registry: mods add theme presets (atmos_api_t.register_theme) after
 * settings_init. Indices 0..5 are the built-ins; registered themes follow, and
 * prefs.theme indexes the combined list. Registering a name twice returns the
 * existing index. */
#define SETTINGS_MOD_THEME_MAX 4
int  settings_theme_count(void);                 /* built-in + registered themes */
const char *settings_theme_name(int index);      /* dropdown label; "?" when out of range */
const settings_theme_t *settings_theme_lookup(int index);   /* clamped to theme 0 when invalid */
int  settings_register_theme(const char *name, const settings_theme_t *theme);   /* index or -1 */
bool settings_select_theme(int index);           /* make theme[index] active now: saved, applied the same frame (the mod API's apply_theme) */
uint8_t settings_pointer_speed(void);           /* 0 = slow, 1 = normal, 2 = fast */
bool settings_clock_24h(void);                  /* false = 12-hour with AM/PM */
bool settings_show_battery(void);                /* menu bar battery indicator */
bool settings_dock_labels(void);                /* tooltip with the app name over the dock */
bool settings_take_changed(void);               /* true once after a change: re-apply the palette */
bool settings_take_font_changed(void);          /* true once after the font changed: redraw everything */

/* Applies the chosen font to graphx. Call after gfx_Begin() and after a font change. */
void settings_apply_font(void);

/* A custom fontlibc font (font pack AppVar, e.g. from cefont) is selected: fills the AppVar name
 * and the font's index inside the pack. False when a built-in font is in use. */
bool settings_custom_font(char var_name[9], uint8_t *font_index);

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
bool settings_cancel(void);                     /* Clear: closes an open dropdown; true = handled */
void settings_flush(void);                      /* archive the AppVar; call before leaving the shell */

#endif