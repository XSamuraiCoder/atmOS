#ifndef SHELL_DESKTOP_H
#define SHELL_DESKTOP_H

#include <stdbool.h>
#include <stdint.h>
#include "files.h"

/* Programs and apps pinned to the desktop (the wallpaper): an icon with its name on a grid,
 * added from the Files pin menu (Alpha), moved by holding 2nd and dragging. Saved in the
 * AppVar SHLDESK, written when the shell is left or a program starts (desktop_flush). */
#define DESK_MAX     12
#define DESK_CELL_W  56
#define DESK_CELL_H  40
#define DESK_X0      6
#define DESK_Y0      20
#define DESK_COLS    5                 /* 6 + 5 * 56 = 286 */
#define DESK_ROWS    4                 /* 20 + 4 * 40 = 180: above the dock */

typedef struct {
    char    name[9];
    uint8_t type, program_kind;
    uint8_t col, row;                  /* grid cell */
} desk_icon_t;

bool desktop_alloc(void);              /* icon sprites (heap); call once at startup */
int  desktop_count(void);
const desk_icon_t *desktop_icon(int i);
int  desktop_find(const char *name, uint8_t type);   /* index or -1 */
/* Add at the cell under (x, y), or the first free cell when x < 0 (or that one is taken).
 * False when the desktop is full. Adding one that is already there just moves it. */
bool desktop_add(const files_pin_t *pin, int x, int y);
void desktop_remove(int i);
/* Move icon i to the cell under (x, y); an icon already there swaps into i's old cell. */
void desktop_move(int i, int x, int y);
int  desktop_hit(int x, int y);        /* icon under the point, or -1 */
void desktop_rect(int i, int r[4]);    /* screen rectangle {x0, y0, x1, y1} of icon i's cell */
/* Icons touching [x0,x1) x [y0,y1); `hover` is highlighted, `hidden` (being dragged) skipped. */
void desktop_draw(int x0, int y0, int x1, int y1, int hover, int hidden);
void desktop_draw_icon(int i, int x, int y);    /* just the 16x16 icon at x, y (drag ghost) */
void desktop_load_icons(void);         /* after a change, with fileioc usable */
bool desktop_take_changed(void);       /* true once after an add / remove / move */
bool desktop_flush(void);              /* write SHLDESK; only on the OS stack, like pins */

#endif
