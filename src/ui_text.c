#include <graphx.h>
#include "settings.h"
#include "ui_text.h"

/* ---- overlay repair (see ui_text.h) ---- */
static int sp_x0, sp_y0, sp_x1, sp_y1;          /* union of custom-font text that left the clip */
static gfx_region_t saved_clip;
static bool clip_widened;

void ui_spill_reset(void)
{
    sp_x0 = sp_y0 = sp_x1 = sp_y1 = 0;
}

static void spill_add(int x0, int y0, int x1, int y1)
{
    if (sp_x1 <= sp_x0) { sp_x0 = x0; sp_y0 = y0; sp_x1 = x1; sp_y1 = y1; return; }
    if (x0 < sp_x0) sp_x0 = x0;
    if (y0 < sp_y0) sp_y0 = y0;
    if (x1 > sp_x1) sp_x1 = x1;
    if (y1 > sp_y1) sp_y1 = y1;
}

bool ui_spill_hits(int x0, int y0, int x1, int y1)
{
    return sp_x1 > sp_x0 && x0 < sp_x1 && sp_x0 < x1 && y0 < sp_y1 && sp_y0 < y1;
}

void ui_overlay_clip(int x0, int y0, int x1, int y1)
{
    saved_clip.xmin = 0;  saved_clip.ymin = 0;
    saved_clip.xmax = GFX_LCD_WIDTH;  saved_clip.ymax = GFX_LCD_HEIGHT;
    gfx_GetClipRegion(&saved_clip);                     /* the clip in force right now */
    clip_widened = true;
    gfx_SetClipRegion(x0, y0, x1, y1);
}

void ui_overlay_clip_end(void)
{
    if (!clip_widened) return;
    clip_widened = false;
    gfx_SetClipRegion(saved_clip.xmin, saved_clip.ymin, saved_clip.xmax, saved_clip.ymax);
}


#ifndef SHELL_NO_FONTLIBC
#include <fontlibc.h>

static bool    custom_active;
static uint8_t custom_h = 8;            /* height of the active custom font */
static uint8_t text_fg = 0;
static int bx0 = 0, by0 = 0, bx1 = GFX_LCD_WIDTH, by1 = GFX_LCD_HEIGHT;   /* hard text limits */

void ui_text_apply(void)
{
    char var[9];
    uint8_t index;

    custom_active = false;
    settings_apply_font();                              /* graphx side (built-in fonts) */

    if (!settings_custom_font(var, &index))
        return;
    {
        fontlib_font_t *font = fontlib_GetFontByIndex(var, index);
        /* fontlib_SetFont exists in every toolchain release (newer ones deprecate it for
         * fontlib_LoadFont); line spacing is zeroed by hand so y is the top of the glyphs */
        if (!font || !fontlib_SetFont(font, 0))
            return;                                     /* pack gone or invalid: built-in font */
        fontlib_SetLineSpacing(0, 0);
    }
    fontlib_SetTransparency(true);
    fontlib_SetNewlineOptions(0);                       /* no wrapping, scrolling or clearing */
    fontlib_SetWindowFullScreen();
    fontlib_SetColors(text_fg, 255);
    custom_h = fontlib_GetCurrentFontHeight();
    if (custom_h == 0) return;
    custom_active = true;
}

void ui_text_color(uint8_t fg)
{
    text_fg = fg;
    gfx_SetTextFGColor(fg);
    if (custom_active) fontlib_SetColors(fg, 255);
}

void ui_text_bounds(int x0, int y0, int x1, int y1)
{
    bx0 = x0; by0 = y0; bx1 = x1; by1 = y1;
}

void ui_print(const char *s, int x, int y)
{
    gfx_region_t r;
    int ty, w;
    bool inside;

    if (!custom_active) {
        gfx_PrintStringXY(s, x, y);
        return;
    }

    ty = y + (8 - (int)custom_h) / 2;                   /* center on the 8px line */
    if (ty < 0) ty = 0;
    w = (int)fontlib_GetStringWidth(s);

    /* Only draw text that touches the area being repainted. */
    r.xmin = x; r.ymin = ty; r.xmax = x + w; r.ymax = ty + (int)custom_h;
    if (!gfx_GetClipRegion(&r)) return;
    inside = r.xmin == x && r.ymin == ty && r.xmax == x + w && r.ymax == ty + (int)custom_h;

    /* ...but draw it whole: fontlib skips glyphs that do not fit its window entirely, so the
     * window is the hard bounds, not the (often tiny) redraw rectangle. Pixels outside the
     * redraw area get the same text they already have, so overdrawing them is harmless. */
    if (x < bx0 || ty < by0 || x >= bx1 || ty + (int)custom_h > by1)
        return;
    if (!inside) spill_add(x, ty, x + w, ty + (int)custom_h);   /* will draw past the clip */
    fontlib_SetWindow((unsigned)bx0, (uint8_t)by0, (unsigned)(bx1 - bx0), (uint8_t)(by1 - by0));
    fontlib_SetCursorPosition((unsigned)x, (uint8_t)ty);
    fontlib_DrawString(s);
}

int ui_text_width(const char *s)
{
    if (custom_active) return (int)fontlib_GetStringWidth(s);
    return (int)gfx_GetStringWidth(s);
}

bool ui_text_custom(void)
{
    return custom_active;
}

uint8_t ui_text_height(void)
{
return custom_active ? custom_h : 8;
}

#else   /* SHELL_NO_FONTLIBC: graphx fonts only */

void ui_text_apply(void)                  { settings_apply_font(); }
void ui_text_bounds(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; }
void ui_text_color(uint8_t fg)            { gfx_SetTextFGColor(fg); }
void ui_print(const char *s, int x, int y){ gfx_PrintStringXY(s, x, y); }
int  ui_text_width(const char *s)         { return (int)gfx_GetStringWidth(s); }
bool ui_text_custom(void)                 { return false; }
uint8_t ui_text_height(void)             { return 8; }

#endif