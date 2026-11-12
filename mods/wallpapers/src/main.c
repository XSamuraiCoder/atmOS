/* main.c - "Walls": picture wallpapers for atmOS (mod API v1.1).
 *
 * Registers three original pictures (Dunes, Lake, Aurora - painted by tools/gen_wallpapers.py)
 * as wallpapers; they appear in Settings > Look > Wallpaper after Gradient, Flat and Reverse.
 * No dock icon or window. Kept out of the shell itself so atmOS stays small: install this
 * AppVar only if you want the pictures.
 *
 * The shell repaints the desktop in small rectangles (each pointer move repaints the pixels
 * under the old pointer), so draw() decodes just the rows and columns it is asked for, straight
 * into the draw buffer: every stored pixel is 2x2 on screen, a run is one fill, and the second
 * screen row of each pair is a copy of the first.
 */
#include <stdbool.h>
#include <stdint.h>
#include "atmos_mod.h"
#include "walls.h"

bool mod_main(const atmos_api_t *api);
ATMOS_MOD_HEADER("Walls", mod_main);

#define SCREEN_W 320

static const atmos_api_t *shell;

/* mods link no C library: no memset / memcpy */
static void fill(uint8_t *dst, uint8_t c, int n)        { while (n-- > 0) *dst++ = c; }
static void copy(uint8_t *dst, const uint8_t *src, int n) { while (n-- > 0) *dst++ = *src++; }

static void draw_image(const wall_image_t *img, int x0, int y0, int x1, int y1)
{
    uint8_t *buf = shell->draw_buffer();
    int y, w = x1 - x0;

    for (y = y0; y < y1; y++) {
        uint8_t *dst = buf + y * SCREEN_W + x0;
        const uint8_t *run;
        int x;

        if ((y & 1) && y > y0) {                    /* same stored row as the line above */
            copy(dst, dst - SCREEN_W, w);
            continue;
        }
        run = img->runs + img->rows[y >> 1];
        for (x = 0; x < x1; run++) {                /* x = screen x where *run starts */
            int end = x + (((*run >> 4) + 1) << 1);
            if (end > x0) {
                int from = x > x0 ? x : x0;
                int to = end < x1 ? end : x1;
                fill(dst + (from - x0), ATMOS_PAL_WALL(*run & 15), to - from);
            }
            x = end;
        }
    }
}

/* draw() gets no context, so one tiny function per picture */
static void draw_dunes(int x0, int y0, int x1, int y1)  { draw_image(&wall_images[0], x0, y0, x1, y1); }
static void draw_lake(int x0, int y0, int x1, int y1)   { draw_image(&wall_images[1], x0, y0, x1, y1); }
static void draw_aurora(int x0, int y0, int x1, int y1) { draw_image(&wall_images[2], x0, y0, x1, y1); }

static void (*const drawers[WALL_IMAGE_COUNT])(int, int, int, int) = {
    draw_dunes, draw_lake, draw_aurora
};

bool mod_main(const atmos_api_t *api)
{
    int i;
    bool any = false;

    if (api->version != ATMOS_API_VERSION || api->minor < 1) return false;
    shell = api;
    for (i = 0; i < WALL_IMAGE_COUNT; i++) {
        atmos_wallpaper_t wp;
        wp.name = wall_images[i].name;
        wp.palette = wall_images[i].palette;
        wp.colors = WALL_COLORS;
        wp.draw = drawers[i];
        if (api->register_wallpaper(&wp) >= 0) any = true;
    }
    return any;                                     /* nothing registered: free the memory */
}
