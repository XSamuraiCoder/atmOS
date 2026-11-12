#ifndef SHELL_WALLPAPERS_H
#define SHELL_WALLPAPERS_H

#include <stdbool.h>
#include <stdint.h>

/* The Wallpaper list (Settings > Look > Wallpaper), in order:
 *   0..2  Gradient, Flat, Reverse - the theme's gradient (settings.c builds those colors)
 *   3..   wallpapers added by mods (atmos_api_t.register_wallpaper, API v1.1), e.g. the
 *         picture wallpapers in mods/wallpapers
 * A mod wallpaper never uses the theme gradient: its colors go into the gradient's palette
 * slots 0..15 plus 246..253, and while it is active slots 30..245 hold a fixed 6x6x6 color
 * cube (the same one files.c uses for program icons), so a picture has ~240 colors (8bpp). */
#define WALL_STYLE_COUNT  3
#define WALL_MOD_MAX      4
#define WALL_MAX          (WALL_STYLE_COUNT + WALL_MOD_MAX)

#define WALLPAPER_COLORS  24
#define WALLPAPER_SLOT(i) ((i) < 16 ? (i) : 246 + (i) - 16)   /* color i -> palette slot */
#define WALLPAPER_CUBE0   30           /* 30 + r*36 + g*6 + b, r/g/b = 0..5 (0, 51, .. 255) */

typedef struct { uint8_t r, g, b; } wallpaper_rgb_t;
typedef void (*wallpaper_draw_fn)(int x0, int y0, int x1, int y1);

int  wallpapers_count(void);                       /* styles + registered mod ones */
const char *wallpapers_name(int index);            /* "?" when out of range */
/* true when `index` shows the theme gradient: a style, or a mod wallpaper that is not loaded */
bool wallpapers_uses_gradient(int index);
int  wallpapers_register(const char *name, const wallpaper_rgb_t *palette, int colors,
                         wallpaper_draw_fn draw);  /* index, or -1 when full / invalid */
void wallpapers_setup_palette(int index);          /* a mod wallpaper's colors; nothing for a style */
/* Has a mod wallpaper paint [x0,x1) x [y0,y1) (clip set to it by the caller). */
void wallpapers_draw(int index, int x0, int y0, int x1, int y1);
uint8_t *wallpapers_draw_buffer(void);             /* buffer graphx draws into, flip finished */

#endif
