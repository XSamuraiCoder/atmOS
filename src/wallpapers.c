/* wallpapers.c - the Wallpaper list: the theme gradient styles plus wallpapers mods register.
 * Pictures are not built in (they would weigh down the app): they come as a mod, see
 * mods/wallpapers. */
#include <graphx.h>
#include <string.h>
#include "wallpapers.h"

static const char *const style_names[WALL_STYLE_COUNT] = { "Gradient", "Flat", "Reverse" };

static struct {
    char              name[16];
    wallpaper_rgb_t   palette[WALLPAPER_COLORS];
    uint8_t           colors;
    wallpaper_draw_fn draw;
} mod_wall[WALL_MOD_MAX];
static uint8_t mod_wall_count;

int wallpapers_count(void)
{
    return WALL_STYLE_COUNT + mod_wall_count;
}

const char *wallpapers_name(int index)
{
    if (index < 0 || index >= wallpapers_count()) return "?";
    return index < WALL_STYLE_COUNT ? style_names[index] : mod_wall[index - WALL_STYLE_COUNT].name;
}

bool wallpapers_uses_gradient(int index)
{
    return index < WALL_STYLE_COUNT || index >= wallpapers_count();
}

int wallpapers_register(const char *name, const wallpaper_rgb_t *palette, int colors,
                        wallpaper_draw_fn draw)
{
    int i;

    if (!name || !name[0] || !draw || colors < 0 || colors > WALLPAPER_COLORS ||
        (colors && !palette))
        return -1;
    for (i = 0; i < mod_wall_count; i++)            /* registering twice returns the same index */
        if (strcmp(mod_wall[i].name, name) == 0)
            return WALL_STYLE_COUNT + i;
    if (mod_wall_count >= WALL_MOD_MAX || strlen(name) >= sizeof(mod_wall[0].name))
        return -1;
    strcpy(mod_wall[mod_wall_count].name, name);
    if (colors) memcpy(mod_wall[mod_wall_count].palette, palette, (size_t)colors * sizeof *palette);
    mod_wall[mod_wall_count].colors = (uint8_t)colors;
    mod_wall[mod_wall_count].draw = draw;
    return WALL_STYLE_COUNT + mod_wall_count++;
}

void wallpapers_setup_palette(int index)
{
    int i;

    if (wallpapers_uses_gradient(index)) return;
    for (i = 0; i < 216; i++)                       /* the fixed cube: same values as files.c */
        gfx_palette[WALLPAPER_CUBE0 + i] = gfx_RGBTo1555(i / 36 * 51, i / 6 % 6 * 51, i % 6 * 51);
    index -= WALL_STYLE_COUNT;
    for (i = 0; i < mod_wall[index].colors; i++)
        gfx_palette[WALLPAPER_SLOT(i)] = gfx_RGBTo1555(mod_wall[index].palette[i].r,
                                                       mod_wall[index].palette[i].g,
                                                       mod_wall[index].palette[i].b);
}

void wallpapers_draw(int index, int x0, int y0, int x1, int y1)
{
    if (x1 <= x0 || y1 <= y0 || wallpapers_uses_gradient(index)) return;
    mod_wall[index - WALL_STYLE_COUNT].draw(x0, y0, x1, y1);
}

/* graphx's own drawing waits until the last gfx_SwapDraw has really happened; a mod writing
 * the buffer directly must too, or it lands in the buffer still on screen (flicker). */
uint8_t *wallpapers_draw_buffer(void)
{
    gfx_Wait();
    return &gfx_vbuffer[0][0];
}
