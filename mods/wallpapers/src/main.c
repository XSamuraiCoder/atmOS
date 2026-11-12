/* main.c - one picture wallpaper + its theme, as a self-contained atmOS mod (API v1.1).
 *
 * Built once per picture (see the Makefile): WPDUNES, WPLAKE and WPAURORA are each a complete
 * mod - install only the ones you want. Each registers:
 *   - a wallpaper (Settings > Look > Wallpaper, after Gradient, Flat and Reverse), and
 *   - a theme of the same name (Settings > Look > Theme) with colors to match, paired with the
 *     wallpaper: picking the theme picks the picture; the wallpaper can still be changed.
 *
 * The AppVar is the mod code, its relocation table, then the picture data (data/<name>.bin from
 * tools/gen_wallpapers.py): "atmOSwp2", name, 24 colors, theme colors, then 160x120 pixels at 8
 * bits per pixel - each byte a palette entry, one of the picture's own colors (ATMOS_PAL_WALL)
 * or the shell's color cube (ATMOS_PAL_CUBE), ~240 colors. Only the code is loaded into RAM;
 * the picture is read in place from the archived AppVar (api->var_data), so it costs no RAM.
 *
 * The shell repaints the desktop in small rectangles (each pointer move repaints the pixels
 * under the old pointer), so draw() copies just the rows and columns it is asked for, straight
 * into the draw buffer: every stored pixel is 2x2 on screen, and the second screen row of each
 * pair is a copy of the first.
 */
#include <stdbool.h>
#include <stdint.h>
#include "atmos_mod.h"

/* PIC selects the build: -DPIC=0/1/2 from the Makefile. The AppVar name must match the
 * Makefile's (the mod finds its own picture data by it). */
#if PIC == 0
#define MOD_VAR   "WPDUNES"
#define MOD_TITLE "Dunes"
#elif PIC == 1
#define MOD_VAR   "WPLAKE"
#define MOD_TITLE "Lake"
#elif PIC == 2
#define MOD_VAR   "WPAURORA"
#define MOD_TITLE "Aurora"
#else
#error "build with -DPIC=0, 1 or 2 (see the Makefile)"
#endif

bool mod_main(const atmos_api_t *api);
ATMOS_MOD_HEADER(MOD_TITLE, mod_main);

#define SCREEN_W   320
#define PIC_W      160
#define PIC_H      120
#define PIC_COLORS 24
#define PIC_NAME   16
#define PIC_THEME  (8 + PIC_NAME + PIC_COLORS * 3)     /* 6 theme colors */
#define PIC_PIXELS (PIC_THEME + 6 * 3)
#define PIC_SIZE   (PIC_PIXELS + PIC_W * PIC_H)

static const atmos_api_t *shell;
/* the shell copies these, but only accepts pointers into a mod: copied here first */
static char        name[PIC_NAME];
static atmos_rgb_t palette[PIC_COLORS];

/* mods link no C library: no memcpy */
static void copy(uint8_t *dst, const uint8_t *src, int n) { while (n-- > 0) *dst++ = *src++; }

static const uint8_t *cached;          /* where the picture was last found */

/* "atmOSwp2" + our name at d? (name is filled in by mod_main; empty until then) */
static bool is_ours(const uint8_t *d)
{
    static const char magic[8] = { 'a', 't', 'm', 'O', 'S', 'w', 'p', '2' };
    int k;

    for (k = 0; k < 8; k++)
        if (d[k] != (uint8_t)magic[k]) return false;
    for (k = 0; k < PIC_NAME && name[k]; k++)
        if (d[8 + k] != (uint8_t)name[k]) return false;
    return true;
}

/* The picture data at the end of this mod's own AppVar, or NULL. Searching the variable table
 * (var_data) on every repaint lags the pointer, so the address is kept and only checked: it
 * stays valid unless archiving a variable garbage collects and moves the archive, and then the
 * check fails and the AppVar is looked up again. */
static const uint8_t *picture(void)
{
    int size;
    const uint8_t *d;

    if (cached && is_ours(cached)) return cached;
    cached = NULL;
    d = shell->var_data(MOD_VAR, &size);
    if (!d || size < PIC_SIZE) return NULL;
    d += size - PIC_SIZE;
    if (!is_ours(d)) return NULL;
    cached = d;
    return d;
}

static void draw(int x0, int y0, int x1, int y1)
{
    const uint8_t *pic = picture();
    uint8_t *buf = shell->draw_buffer();
    int y, w = x1 - x0;

    for (y = y0; y < y1; y++) {
        uint8_t *dst = buf + y * SCREEN_W + x0;
        int x;

        if ((y & 1) && y > y0) {                    /* same stored row as the line above */
            copy(dst, dst - SCREEN_W, w);
            continue;
        }
        if (!pic) {                                 /* AppVar renamed meanwhile: plain fill */
            for (x = x0; x < x1; x++) *dst++ = ATMOS_PAL_TEXT;
            continue;
        }
        {
            const uint8_t *src = pic + PIC_PIXELS + (y >> 1) * PIC_W + (x0 >> 1);
            x = x0;
            if (x & 1) { *dst++ = *src++; x++; }    /* right half of a stored pixel */
            for (; x + 1 < x1; x += 2) {
                uint8_t c = *src++;
                dst[0] = c;
                dst[1] = c;
                dst += 2;
            }
            if (x < x1) *dst = *src;                /* left half of the last one */
        }
    }
}

static atmos_rgb_t rgb(const uint8_t *p)
{
    atmos_rgb_t c;
    c.r = p[0]; c.g = p[1]; c.b = p[2];
    return c;
}

bool mod_main(const atmos_api_t *api)
{
    const uint8_t *pic, *t;
    atmos_wallpaper_t wp;
    atmos_theme_t th;
    int wall, theme;

    if (api->version != ATMOS_API_VERSION || api->minor < 1) return false;
    shell = api;
    pic = picture();
    if (!pic) return false;                         /* data missing: free the mod's memory */

    copy((uint8_t *)name, pic + 8, PIC_NAME);
    name[PIC_NAME - 1] = 0;
    copy((uint8_t *)palette, pic + 8 + PIC_NAME, PIC_COLORS * 3);
    wp.name = name;
    wp.palette = palette;
    wp.colors = PIC_COLORS;
    wp.draw = draw;
    wall = api->register_wallpaper(&wp);
    if (wall < 0) return false;                     /* wallpaper list full */

    t = pic + PIC_THEME;
    th.name = name;
    th.top = rgb(t);
    th.bottom = rgb(t + 3);
    th.bar = rgb(t + 6);
    th.bar_line = rgb(t + 9);
    th.dock = rgb(t + 12);
    th.accent = rgb(t + 15);
    theme = api->register_theme(&th);
    if (theme >= 0) api->set_theme_wallpaper(theme, wall);
    return true;
}
