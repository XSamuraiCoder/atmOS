#ifndef SHELL_UI_TEXT_H
#define SHELL_UI_TEXT_H

#include <stdbool.h>
#include <stdint.h>

/* All shell text goes through here so a custom fontlibc font (a font pack AppVar, for example
 * one made with cefont) can replace the built-in graphx font. The built-in path is exactly the
 * old graphx behavior, so with no custom font selected nothing changes.
 *
 *  ui_text_apply()  after gfx_Begin() and whenever the font setting changes
 *  ui_text_color()  foreground color; the background is always transparent
 *  ui_text_bounds() hard limits for text (default: the screen). The shell sets this to the
 *                   app window's content area while an app draws
 *  ui_print()       draw at x, y where y is the top of an 8px line, as with gfx_PrintStringXY;
 *                   taller or shorter fonts are centered on that line. A custom font is drawn
 *                   whole (fontlib cannot clip part of a glyph) whenever it touches the current
 *                   graphx clip region, so partial redraws around the pointer never cut glyphs;
 *                   it is only limited by ui_text_bounds()
 *  ui_text_width()  width in pixels
 *
 * Build with -DSHELL_NO_FONTLIBC to drop fontlibc entirely (graphx fonts only). */
void ui_text_apply(void);
void ui_text_bounds(int x0, int y0, int x1, int y1);
void ui_text_color(uint8_t fg);
void ui_print(const char *s, int x, int y);
int  ui_text_width(const char *s);
bool ui_text_custom(void);              /* a custom font is active */

/* Overlays (dropdown list, context menu) are drawn on top of other content. A custom font is
 * drawn whole, so text under an overlay can spill past the partial-repaint rectangle onto the
 * overlay's pixels. The fix: note where such text went, and if it reached the overlay, repaint
 * the overlay completely with the graphx clip widened to its rectangle.
 *
 *   ui_spill_reset();                      start of the app's draw_region
 *   ...draw the content under the overlay...
 *   if (in_region || ui_spill_hits(rect)) {
 *       if (ui_spill_hits(rect)) ui_overlay_clip(rect);
 *       draw_overlay();
 *       ui_overlay_clip_end();             (no-op when ui_overlay_clip was not called)
 *   }
 * Rectangles are x0, y0, x1, y1 with x1/y1 exclusive. */
void ui_spill_reset(void);
bool ui_spill_hits(int x0, int y0, int x1, int y1);
void ui_overlay_clip(int x0, int y0, int x1, int y1);
void ui_overlay_clip_end(void);
uint8_t ui_text_height(void);            /* pixel height of the active font (8 = built-in) */

#endif