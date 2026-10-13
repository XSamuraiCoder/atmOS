/* settings.c - built-in Settings app for the TI-84 Plus CE shell
 *
 * Pointer-first, like Files: click a button (Enter or 2nd) and the change applies
 * immediately, so the wallpaper, menu bar and dock behind the window act as the
 * preview. Settings are kept in the AppVar SHLSET (RAM on every change, archived
 * when the shell is left).
 */
#include <graphx.h>
#include <fileioc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "settings.h"

/* palette indices (same as main.c / files.c / notes.c) */
enum {
    PAL_BAR = 16, PAL_BAR_LINE = 17, PAL_DOCK = 18,
    PAL_TEXT = 19, PAL_LABEL = 20, PAL_ACCENT = 21,
    PAL_ICON0 = 22                      /* red,yl,grn,blu,pur,cyn,org,gray */
};

#define WIN_W       304
#define CONTENT_H   180

#define STORE_NAME     "SHLSET"
#define STORE_VERSION  1
#define STORE_SIZE     4                /* version, theme, speed, clock24 */

/* ---------- theme presets ---------- */
#define THEME_COUNT 6

static const char *const theme_names[THEME_COUNT] = {
    "Sunset", "Ocean", "Forest", "Grape", "Mono", "Candy"
};

/* the text color is fixed (dark), so bars and docks stay light in every preset */
static const settings_theme_t themes[THEME_COUNT] = {
    /* Sunset (the original look) */
    { {  60, 100, 190 }, { 200, 120, 170 }, { 236, 236, 240 }, { 190, 190, 198 },
      { 225, 225, 235 }, {   0, 122, 255 } },
    /* Ocean */
    { {  10,  60, 120 }, {  70, 190, 210 }, { 232, 240, 244 }, { 176, 196, 206 },
      { 220, 232, 238 }, {   0, 150, 199 } },
    /* Forest */
    { {  24,  70,  52 }, { 150, 190, 110 }, { 236, 240, 232 }, { 184, 196, 176 },
      { 226, 232, 220 }, {  40, 160,  80 } },
    /* Grape */
    { {  60,  30, 110 }, { 220, 110, 170 }, { 240, 234, 244 }, { 196, 184, 206 },
      { 232, 224, 238 }, { 150,  70, 220 } },
    /* Mono */
    { {  40,  40,  46 }, { 150, 150, 160 }, { 238, 238, 240 }, { 190, 190, 194 },
      { 226, 226, 230 }, { 100, 110, 130 } },
    /* Candy */
    { { 250, 120, 160 }, { 255, 210, 120 }, { 250, 238, 240 }, { 206, 186, 192 },
      { 244, 228, 232 }, { 240,  70, 120 } }
};

/* ---------- state ---------- */
typedef struct {
    uint8_t theme;      /* index into themes[] */
    uint8_t speed;      /* 0 slow, 1 normal, 2 fast */
    uint8_t clock24;    /* 1 = 24-hour, 0 = 12-hour */
} prefs_t;

static const prefs_t defaults = { 0, 1, 1 };

static prefs_t prefs = { 0, 1, 1 };
static bool    changed;                 /* a setting changed; main.c re-applies it */
static bool    archive_pending;
static int     hover = -1;              /* button under the pointer, -1 = none */
static uint16_t dirty_mask;             /* buttons whose look changed since settings_dirty() */

/* ---------- storage ---------- */
static void load_prefs(void)
{
    uint8_t buf[STORE_SIZE];
    uint8_t h = ti_Open(STORE_NAME, "r");

    prefs = defaults;
    if (!h) return;
    if (ti_Read(buf, 1, STORE_SIZE, h) == STORE_SIZE &&
        buf[0] == STORE_VERSION &&
        buf[1] < THEME_COUNT && buf[2] < 3 && buf[3] < 2) {
        prefs.theme   = buf[1];
        prefs.speed   = buf[2];
        prefs.clock24 = buf[3];
    }
    ti_Close(h);
}

static void store_prefs(void)
{
    uint8_t buf[STORE_SIZE];
    uint8_t h;

    buf[0] = STORE_VERSION;
    buf[1] = prefs.theme;
    buf[2] = prefs.speed;
    buf[3] = prefs.clock24;

    h = ti_Open(STORE_NAME, "w");
    if (!h) return;
    ti_Write(buf, 1, STORE_SIZE, h);
    ti_Close(h);
    archive_pending = true;
}

void settings_init(void)
{
    load_prefs();
    changed = false;
    hover = -1;
}

void settings_flush(void)
{
    uint8_t h;
    if (!archive_pending) return;
    h = ti_Open(STORE_NAME, "r");
    if (h) { ti_SetArchiveStatus(true, h); ti_Close(h); }
    archive_pending = false;
}

/* ---------- values for the shell ---------- */
const settings_theme_t *settings_theme(void)
{
    return &themes[prefs.theme < THEME_COUNT ? prefs.theme : 0];
}

uint8_t settings_pointer_speed(void)
{
    return prefs.speed < 3 ? prefs.speed : 1;
}

bool settings_clock_24h(void)
{
    return prefs.clock24 != 0;
}

bool settings_take_changed(void)
{
    bool c = changed;
    changed = false;
    return c;
}

/* ---------- layout ----------
 * Buttons are numbered: 0-5 themes (3x2 grid), 6-8 pointer speed,
 * 9-10 clock format, 11 reset. Everything is a 94px-wide button. */
#define BTN_W    94
#define BTN_GAP  5
#define BTN_X0   6

#define ID_SPEED0   THEME_COUNT
#define ID_CLOCK24  (ID_SPEED0 + 3)
#define ID_CLOCK12  (ID_CLOCK24 + 1)
#define ID_RESET    (ID_CLOCK12 + 1)
#define BTN_COUNT   (ID_RESET + 1)
#define BIT(id)     ((uint16_t)1u << (id))

typedef struct { int x, y, w, h; } rect_t;

static rect_t btn_rect(int id)
{
    rect_t r = { 0, 0, BTN_W, 20 };

    if (id < THEME_COUNT) {
        r.x = BTN_X0 + (id % 3) * (BTN_W + BTN_GAP);
        r.y = 18 + (id / 3) * 27;
        r.h = 22;
    } else if (id < ID_CLOCK24) {
        r.x = BTN_X0 + (id - ID_SPEED0) * (BTN_W + BTN_GAP);
        r.y = 88;
    } else {
        r.x = BTN_X0 + (id - ID_CLOCK24) * (BTN_W + BTN_GAP);
        r.y = 130;
    }
    return r;
}

static const char *btn_label(int id)
{
    static const char *const speed_names[3] = { "Slow", "Normal", "Fast" };

    if (id < THEME_COUNT) return theme_names[id];
    if (id < ID_CLOCK24)  return speed_names[id - ID_SPEED0];
    if (id == ID_CLOCK24) return "24-hour";
    if (id == ID_CLOCK12) return "12-hour";
    return "Reset all";
}

static bool btn_selected(int id)
{
    if (id < THEME_COUNT) return prefs.theme == id;
    if (id < ID_CLOCK24)  return prefs.speed == id - ID_SPEED0;
    if (id == ID_CLOCK24) return prefs.clock24 != 0;
    if (id == ID_CLOCK12) return prefs.clock24 == 0;
    return false;
}

static int btn_at(int x, int y)
{
    int id;
    for (id = 0; id < BTN_COUNT; id++) {
        rect_t r = btn_rect(id);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return id;
    }
    return -1;
}

/* ---------- drawing ---------- */
static void text_color(uint8_t fg)
{
    gfx_SetTextFGColor(fg);
    gfx_SetTextBGColor(255);
    gfx_SetTextTransparentColor(255);
}

static void fill_rr(int x, int y, int w, int h, int r)
{
    gfx_FillRectangle(x + r, y, w - 2 * r, h);
    gfx_FillRectangle(x, y + r, w, h - 2 * r);
    gfx_FillCircle(x + r,         y + r,         r);
    gfx_FillCircle(x + w - r - 1, y + r,         r);
    gfx_FillCircle(x + r,         y + h - r - 1, r);
    gfx_FillCircle(x + w - r - 1, y + h - r - 1, r);
}

static void draw_button(int x, int y, int id)
{
    rect_t r = btn_rect(id);
    const char *s = btn_label(id);
    bool sel = btn_selected(id);
    bool hov = id == hover;
    uint8_t bg, fg;

    if (id == ID_RESET) {
        bg = hov ? PAL_ICON0 : PAL_ICON0 + 7;       /* red on hover, otherwise gray */
        fg = PAL_LABEL;
    } else if (sel) {
        bg = PAL_ACCENT;
        fg = PAL_LABEL;
    } else {
        bg = hov ? PAL_LABEL : PAL_BAR;
        fg = PAL_TEXT;
    }

    gfx_SetColor(bg);
    fill_rr(x + r.x, y + r.y, r.w, r.h, 6);
    text_color(fg);
    gfx_PrintStringXY(s, x + r.x + (r.w - gfx_GetStringWidth(s)) / 2,
                         y + r.y + (r.h - 8) / 2);
}

static bool touches(int ax0, int ay0, int ax1, int ay1,
                    int bx0, int by0, int bx1, int by1)
{
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

/* draws only the labels and buttons that overlap [x0,x1) x [y0,y1) (screen coordinates) */
void settings_draw_region(int x, int y, int x0, int y0, int x1, int y1)
{
    int id;

    gfx_SetTextScale(1, 1);

    if (touches(x0, y0, x1, y1, x, y + 6, x + WIN_W, y + 14) ||
        touches(x0, y0, x1, y1, x, y + 76, x + WIN_W, y + 84) ||
        touches(x0, y0, x1, y1, x, y + 118, x + WIN_W, y + 126)) {
        text_color(PAL_TEXT);
        gfx_PrintStringXY("Theme",         x + BTN_X0, y + 6);
        gfx_PrintStringXY("Pointer speed", x + BTN_X0, y + 76);
        gfx_PrintStringXY("Clock",         x + BTN_X0, y + 118);
    }

    for (id = 0; id < BTN_COUNT; id++) {
        rect_t r = btn_rect(id);
        if (touches(x0, y0, x1, y1, x + r.x, y + r.y, x + r.x + r.w, y + r.y + r.h))
            draw_button(x, y, id);
    }

    if (touches(x0, y0, x1, y1, x, y + CONTENT_H - 10, x + WIN_W, y + CONTENT_H - 2)) {
        text_color(PAL_ICON0 + 7);
        gfx_PrintStringXY("Changes apply right away and are saved.",
                          x + BTN_X0, y + CONTENT_H - 10);
    }
}

void settings_draw(int x, int y)
{
    settings_draw_region(x, y, x, y, x + WIN_W, y + CONTENT_H);
}

/* ---------- interaction ---------- */
void settings_open(void)
{
    hover = -1;
    dirty_mask = 0;
}

bool settings_hover(int x, int y)
{
    int id = btn_at(x, y);
    if (id == hover) return false;
    if (hover >= 0) dirty_mask |= BIT(hover);       /* un-highlight the old button */
    if (id >= 0)    dirty_mask |= BIT(id);          /* highlight the new one */
    hover = id;
    return true;
}

int settings_dirty(int x, int y, int r[][4], int max)
{
    uint16_t m = dirty_mask;
    int id, n = 0;

    dirty_mask = 0;
    for (id = 0; id < BTN_COUNT; id++) {
        rect_t b;
        if (!(m & BIT(id))) continue;
        if (n >= max) return -1;                    /* too many pieces: repaint the window */
        b = btn_rect(id);
        r[n][0] = x + b.x;
        r[n][1] = y + b.y;
        r[n][2] = x + b.x + b.w;
        r[n][3] = y + b.y + b.h;
        n++;
    }
    return n;
}

static void apply(const prefs_t *next)
{
    bool before[BTN_COUNT];
    int id;

    if (next->theme == prefs.theme && next->speed == prefs.speed &&
        next->clock24 == prefs.clock24)
        return;                         /* nothing changed: skip the flash write */
    for (id = 0; id < BTN_COUNT; id++) before[id] = btn_selected(id);
    prefs = *next;
    for (id = 0; id < BTN_COUNT; id++)  /* only buttons whose selection flipped need repainting */
        if (btn_selected(id) != before[id]) dirty_mask |= BIT(id);
    changed = true;
    store_prefs();
}

void settings_click(int x, int y)
{
    int id = btn_at(x, y);
    prefs_t next = prefs;

    if (id < 0) return;
    if (id < THEME_COUNT)        next.theme = (uint8_t)id;
    else if (id < ID_CLOCK24)    next.speed = (uint8_t)(id - ID_SPEED0);
    else if (id == ID_CLOCK24)   next.clock24 = 1;
    else if (id == ID_CLOCK12)   next.clock24 = 0;
    else                         next = defaults;
    apply(&next);
}

bool settings_keys(void)
{
    return false;                       /* pointer-first: arrows move the pointer */
}

bool settings_press(void)
{
    return false;                       /* Enter / 2nd click the hovered button */
}