/* desktop.c - icons pinned to the desktop (see desktop.h). */
#include <graphx.h>
#include <fileioc.h>
#include <stdlib.h>
#include <string.h>
#include "desktop.h"
#include "ui_text.h"
#include "ui_text.h"
#define fill_round ui_fill_round   /* shared copy in ui_text.c */

/* palette indices (same as main.c) */
enum { PAL_BAR = 16, PAL_TEXT = 19, PAL_LABEL = 20, PAL_ACCENT = 21, PAL_ICON0 = 22 };

#define STORE_NAME    "SHLDESK"
#define STORE_VERSION 1
#define RECORD_SIZE   12               /* type, kind, col, row, name (8) */

static desk_icon_t icons[DESK_MAX];
static int count;
static bool loaded, changed, dirty;

/* icon sprites (2-byte header + 16x16) live in the heap, not .bss (see files.c) */
static uint8_t (*sprites)[2 + 16 * 16];
static bool has_sprite[DESK_MAX];

bool desktop_alloc(void)
{
    if (!sprites) sprites = calloc(DESK_MAX, sizeof *sprites);
    return sprites != NULL;
}

static void load(void)
{
    uint8_t h, head[6], rec[RECORD_SIZE];
    int i, n;

    if (loaded) return;
    loaded = true;
    count = 0;
    h = ti_Open(STORE_NAME, "r");
    if (!h) return;
    if (ti_Read(head, 1, 6, h) == 6 && memcmp(head, "SHDK", 4) == 0 &&
        head[4] == STORE_VERSION && head[5] <= DESK_MAX) {
        n = head[5];
        for (i = 0; i < n && ti_Read(rec, 1, RECORD_SIZE, h) == RECORD_SIZE; i++) {
            desk_icon_t *d = &icons[count];
            d->type = rec[0];
            d->program_kind = rec[1];
            d->col = rec[2];
            d->row = rec[3];
            memcpy(d->name, rec + 4, 8);
            d->name[8] = 0;
            if (d->col < DESK_COLS && d->row < DESK_ROWS && d->name[0] >= 'A' && d->name[0] <= 'Z')
                count++;
        }
    }
    ti_Close(h);
}

int desktop_count(void)                { load(); return count; }
const desk_icon_t *desktop_icon(int i) { load(); return i >= 0 && i < count ? &icons[i] : NULL; }

int desktop_find(const char *name, uint8_t type)
{
    int i;
    load();
    for (i = 0; i < count; i++)
        if (icons[i].type == type && strcmp(icons[i].name, name) == 0) return i;
    return -1;
}

static int at_cell(int col, int row)
{
    int i;
    for (i = 0; i < count; i++)
        if (icons[i].col == col && icons[i].row == row) return i;
    return -1;
}

static void cell_of(int x, int y, int *col, int *row)
{
    *col = (x - DESK_X0) / DESK_CELL_W;
    *row = (y - DESK_Y0) / DESK_CELL_H;
    if (x < DESK_X0) *col = 0;
    if (y < DESK_Y0) *row = 0;
    if (*col >= DESK_COLS) *col = DESK_COLS - 1;
    if (*row >= DESK_ROWS) *row = DESK_ROWS - 1;
}

static void touched(void) { changed = dirty = true; }

bool desktop_add(const files_pin_t *pin, int x, int y)
{
    int col, row, i = desktop_find(pin->name, pin->type);

    if (i >= 0) {                                   /* already there: move it */
        if (x >= 0) desktop_move(i, x, y);
        return true;
    }
    if (count >= DESK_MAX) return false;
    if (x >= 0) cell_of(x, y, &col, &row);
    if (x < 0 || at_cell(col, row) >= 0) {          /* first free cell, column by column */
        for (row = 0; row < DESK_ROWS; row++) {     /* left to right, then down */
            for (col = 0; col < DESK_COLS; col++)
                if (at_cell(col, row) < 0) break;
            if (col < DESK_COLS) break;
        }
        if (row >= DESK_ROWS) return false;
    }
    strcpy(icons[count].name, pin->name);
    icons[count].type = pin->type;
    icons[count].program_kind = pin->program_kind;
    icons[count].col = (uint8_t)col;
    icons[count].row = (uint8_t)row;
    count++;
    touched();
    return true;
}

void desktop_remove(int i)
{
    if (i < 0 || i >= count) return;
    for (; i + 1 < count; i++) icons[i] = icons[i + 1];
    count--;
    touched();
}

void desktop_move(int i, int x, int y)
{
    int col, row, other;

    if (i < 0 || i >= count) return;
    cell_of(x, y, &col, &row);
    if (col == icons[i].col && row == icons[i].row) return;
    other = at_cell(col, row);
    if (other >= 0) {                               /* swap places */
        icons[other].col = icons[i].col;
        icons[other].row = icons[i].row;
    }
    icons[i].col = (uint8_t)col;
    icons[i].row = (uint8_t)row;
    touched();
}

void desktop_rect(int i, int r[4])
{
    r[0] = DESK_X0 + icons[i].col * DESK_CELL_W;
    r[1] = DESK_Y0 + icons[i].row * DESK_CELL_H;
    r[2] = r[0] + DESK_CELL_W;
    r[3] = r[1] + DESK_CELL_H;
}

int desktop_hit(int x, int y)
{
    int i, r[4];
    load();
    for (i = 0; i < count; i++) {
        desktop_rect(i, r);
        if (x >= r[0] + 4 && x < r[2] - 4 && y >= r[1] && y < r[3] - 2) return i;
    }
    return -1;
}


void desktop_draw_icon(int i, int x, int y)
{
    const desk_icon_t *d = &icons[i];

    if (has_sprite[i]) {
        gfx_TransparentSprite((gfx_sprite_t *)sprites[i], x, y);
    } else {                                        /* colored square with the first letter */
        char letter[2] = { d->name[0], 0 };
        gfx_SetColor(PAL_ICON0 + (d->type == FILES_TYPE_APP ? 4 : d->type == 0x06 ? 6 : 3));
        fill_round(x, y, 16, 16, 5);
        ui_text_color(PAL_LABEL);                   /* the chosen font, centered */
        ui_print(letter, x + (16 - ui_text_width(letter)) / 2, y + 4);
    }
}

/* The name as it fits its cell (pill padding included): with a wide font a long name is cut
 * and ends in "..", so it never runs into the neighbor's label. */
#define LABEL_MAX_W (DESK_CELL_W - 8)
static void short_name(char out[12], const char *name)
{
    int n = (int)strlen(name);

    strcpy(out, name);
    if (ui_text_width(out) <= LABEL_MAX_W) return;
    while (n > 1) {
        out[--n] = 0;
        strcpy(out + n, "..");
        if (ui_text_width(out) <= LABEL_MAX_W) return;
        out[n] = 0;
    }
    strcpy(out + n, "..");
}

void desktop_draw(int x0, int y0, int x1, int y1, int hover, int hidden)
{
    int i, r[4];

    load();
    gfx_SetTextBGColor(255);                        /* transparent text background */
    gfx_SetTextTransparentColor(255);
    gfx_SetTextScale(1, 1);
    ui_text_bounds(0, DESK_Y0 - 4, 320, DESK_Y0 + DESK_ROWS * DESK_CELL_H + 4);
    for (i = 0; i < count; i++) {
        char label[12];
        int cx, tw, th;
        if (i == hidden) continue;
        desktop_rect(i, r);
        if (r[2] <= x0 || r[0] >= x1 || r[3] <= y0 || r[1] >= y1) continue;
        cx = (r[0] + r[2]) / 2;
        if (i == hover) {
            gfx_SetColor(PAL_ACCENT);
            fill_round(cx - 12, r[1] + 1, 24, 22, 6);
        }
        desktop_draw_icon(i, cx - 8, r[1] + 4);
        /* Name on a pill in the chosen font, readable on any picture. A custom font is drawn
         * whole when it touches the repaint area, so it is kept inside the desktop band (never
         * onto the menu bar or dock); the shell does not draw the desktop under a window. */
        short_name(label, icons[i].name);
        tw = ui_text_width(label);
        th = (int)ui_text_height();
        gfx_SetColor(i == hover ? PAL_ACCENT : PAL_LABEL);
        fill_round(cx - tw / 2 - 3, r[1] + 24, tw + 6, th + 4, 5);
        ui_text_color(i == hover ? PAL_LABEL : PAL_TEXT);
        ui_print(label, cx - tw / 2, r[1] + 26 + (th - 8) / 2);
    }
    ui_text_bounds(0, 0, 320, 240);
}

void desktop_load_icons(void)
{
    int i;
    load();
    for (i = 0; i < count; i++) {
        files_pin_t p;
        sprites[i][0] = sprites[i][1] = 16;
        strcpy(p.name, icons[i].name);
        p.type = icons[i].type;
        p.program_kind = icons[i].program_kind;
        /* like the dock: no Flash App icons (OS-version-specific entry points) */
        has_sprite[i] = p.type != FILES_TYPE_APP && files_load_pin_icon(&p, sprites[i] + 2);
    }
}

bool desktop_take_changed(void)
{
    bool c = changed;
    changed = false;
    return c;
}

bool desktop_flush(void)
{
    uint8_t h, head[6] = { 'S', 'H', 'D', 'K', STORE_VERSION, 0 }, rec[RECORD_SIZE];
    int i;
    bool ok = true;

    if (!dirty) return true;
    head[5] = (uint8_t)count;
    h = ti_Open(STORE_NAME, "w");
    if (!h) return false;
    ok = ti_Write(head, 1, 6, h) == 6;
    for (i = 0; ok && i < count; i++) {
        rec[0] = icons[i].type;
        rec[1] = icons[i].program_kind;
        rec[2] = icons[i].col;
        rec[3] = icons[i].row;
        memcpy(rec + 4, icons[i].name, 8);
        ok = ti_Write(rec, 1, RECORD_SIZE, h) == RECORD_SIZE;
    }
    if (ok) ti_SetArchiveStatus(true, h);
    ti_Close(h);
    if (ok) dirty = false;
    return ok;
}
