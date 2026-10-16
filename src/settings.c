/* settings.c - built-in Settings app for atmOS (TI-84 Plus CE)
 *
 * Three tabs keep every page short:
 *   Look     theme preset, wallpaper style, accent color, font
 *   Taskbar  menu bar color, dock color, battery indicator, dock labels
 *   System   pointer speed, clock format, reset
 * Choices with many values are dropdown menus (click to open, click an item, Clear
 * closes). Pointer-first like Files: Enter or 2nd clicks, and changes apply right away,
 * so the shell behind the window is the preview. Settings live in the AppVar SHLSET
 * (RAM on every change, archived when the shell is left).
 */
#include <graphx.h>
#include <fileioc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "settings.h"
#include "ui_text.h"
#ifndef SHELL_NO_FONTLIBC
#include <fontlibc.h>
#endif
#include "debug_stack.h"

/* palette indices (same as main.c / files.c / notes.c) */
enum {
    PAL_BAR = 16, PAL_BAR_LINE = 17, PAL_DOCK = 18,
    PAL_TEXT = 19, PAL_LABEL = 20, PAL_ACCENT = 21,
    PAL_ICON0 = 22                      /* red,yl,grn,blu,pur,cyn,org,gray */
};
#define PAL_GRAD0 0

#define WIN_W       304
#define CONTENT_H   180

#define STORE_NAME     "SHLSET"
#define STORE_VERSION  3
#define STORE_SIZE     21   /* version, theme, wall, accent, font, bar, dock, speed, clock24, battery, tips,
                         * custom font AppVar name (9), custom font index */
#define STORE_SIZE_V2  11

/* ---------- choices ---------- */
#define THEME_COUNT 6
static const char *const theme_names[THEME_COUNT] = {
    "Sunset", "Ocean", "Forest", "Grape", "Mono", "Candy"
};

/* the text color is fixed (dark), so bars and docks stay light in every preset */
static const settings_theme_t themes[THEME_COUNT] = {
    { {  60, 100, 190 }, { 200, 120, 170 }, { 236, 236, 240 }, { 190, 190, 198 },
      { 225, 225, 235 }, {   0, 122, 255 } },                                   /* Sunset */
    { {  10,  60, 120 }, {  70, 190, 210 }, { 232, 240, 244 }, { 176, 196, 206 },
      { 220, 232, 238 }, {   0, 150, 199 } },                                   /* Ocean */
    { {  24,  70,  52 }, { 150, 190, 110 }, { 236, 240, 232 }, { 184, 196, 176 },
      { 226, 232, 220 }, {  40, 160,  80 } },                                   /* Forest */
    { {  60,  30, 110 }, { 220, 110, 170 }, { 240, 234, 244 }, { 196, 184, 206 },
      { 232, 224, 238 }, { 150,  70, 220 } },                                   /* Grape */
    { {  40,  40,  46 }, { 150, 150, 160 }, { 238, 238, 240 }, { 190, 190, 194 },
      { 226, 226, 230 }, { 100, 110, 130 } },                                   /* Mono */
    { { 250, 120, 160 }, { 255, 210, 120 }, { 250, 238, 240 }, { 206, 186, 192 },
      { 244, 228, 232 }, { 240,  70, 120 } }                                    /* Candy */
};

#define WALL_COUNT 3
static const char *const wall_names[WALL_COUNT] = { "Gradient", "Flat", "Reverse" };

/* menu bar / dock tints (light, so the dark text stays readable); index 0 = follow the theme */
#define TINT_COUNT 7
static const struct { const char *name; settings_rgb_t c; } tints[TINT_COUNT] = {
    { "White",    { 248, 248, 250 } }, { "Silver",   { 222, 222, 228 } },
    { "Cream",    { 250, 244, 226 } }, { "Mint",     { 220, 242, 230 } },
    { "Sky",      { 218, 234, 250 } }, { "Rose",     { 250, 226, 232 } },
    { "Lavender", { 236, 228, 250 } }
};

/* accent colors reuse the shell's icon palette (pal = offset from PAL_ICON0), so the
 * swatches need no extra palette entries; index 0 = follow the theme */
#define ACCENT_COUNT 7
static const struct { const char *name; settings_rgb_t c; uint8_t pal; } accents[ACCENT_COUNT] = {
    { "Red",    { 255,  95,  86 }, 0 }, { "Orange", { 255, 159,  10 }, 6 },
    { "Green",  {  39, 201,  63 }, 2 }, { "Blue",   {  10, 132, 255 }, 3 },
    { "Purple", { 191,  90, 242 }, 4 }, { "Cyan",   { 100, 210, 255 }, 5 },
    { "Gray",   { 142, 142, 147 }, 7 }
};

#define FONT_COUNT 4
enum { FONT_DEFAULT, FONT_COMPACT, FONT_ROOMY, FONT_MONO };
#define FONT_CUSTOM FONT_COUNT          /* prefs.font value: use the custom font in cvar/cidx */
#define MAX_CUSTOM  8                   /* custom fonts listed in the dropdown */
static const char *const font_names[FONT_COUNT] = { "Default", "Compact", "Roomy", "Mono" };

/* ---------- state ---------- */
typedef struct {
    uint8_t theme;      /* index into themes[] */
    uint8_t wall;       /* 0 gradient, 1 flat, 2 reverse */
    uint8_t accent;     /* 0 = theme, else accents[accent - 1] */
    uint8_t font;       /* FONT_*, or FONT_CUSTOM */
    uint8_t bar;        /* 0 = theme, else tints[bar - 1] */
    uint8_t dock;       /* 0 = theme, else tints[dock - 1] */
    uint8_t speed;      /* 0 slow, 1 normal, 2 fast */
    uint8_t clock24;    /* 1 = 24-hour, 0 = 12-hour */
    uint8_t battery;    /* 1 = show */
    uint8_t tips;       /* 1 = dock labels */
    char    cvar[9];    /* custom font: font pack AppVar name */
    uint8_t cidx;       /* custom font: index inside the pack */
} prefs_t;

static const prefs_t defaults = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, { 0 }, 0 };

static prefs_t prefs = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, { 0 }, 0 };
static settings_theme_t eff;            /* preset + overrides; what the shell draws with */
static bool changed;                    /* a setting changed; main.c re-applies the palette */
static bool font_changed;               /* ...and the font changed too */
static bool archive_pending;

/* ---------- widgets ---------- */
enum {
    W_TAB0, W_TAB1, W_TAB2,
    W_THEME, W_WALL, W_ACCENT, W_FONT,              /* Look */
    W_BAR, W_DOCK,                                  /* Taskbar dropdowns */
    W_BATT_ON, W_BATT_OFF, W_TIPS_ON, W_TIPS_OFF,   /* Taskbar buttons */
    W_SPEED0, W_SPEED1, W_SPEED2, W_CLK24, W_CLK12, W_RESET,   /* System */
    W_COUNT
};
#define TAB_ALL (-1)
#define TAB_COUNT 3
#define IS_DROP(id) ((id) >= W_THEME && (id) <= W_DOCK)

typedef struct { int8_t tab; int16_t x, y, w, h; } widget_t;

static const widget_t wd[W_COUNT] = {
    { TAB_ALL,   6, 4, 96, 18 }, { TAB_ALL, 107, 4, 96, 18 }, { TAB_ALL, 208, 4, 90, 18 },
    { 0,   6, 41, 142, 18 }, { 0, 156, 41, 142, 18 },
    { 0,   6, 83, 142, 18 }, { 0, 156, 83, 142, 18 },
    { 1,   6, 41, 142, 18 }, { 1, 156, 41, 142, 18 },
    { 1,   6, 83,  69, 18 }, { 1,  79, 83,  69, 18 },
    { 1, 156, 83,  69, 18 }, { 1, 229, 83,  69, 18 },
    { 2,   6, 41,  94, 20 }, { 2, 105, 41,  94, 20 }, { 2, 204, 41,  94, 20 },
    { 2,   6, 83, 142, 20 }, { 2, 156, 83, 142, 20 },
    { 2,   6, 125, 142, 20 }
};

static const struct { int8_t tab; int16_t x, y; const char *s; } labels[] = {
    { 0,   6, 30, "Theme" },          { 0, 156, 30, "Wallpaper" },
    { 0,   6, 72, "Accent color" },   { 0, 156, 72, "Font" },
    { 1,   6, 30, "Menu bar color" }, { 1, 156, 30, "Dock color" },
    { 1,   6, 72, "Battery indicator" }, { 1, 156, 72, "Dock labels" },
    { 2,   6, 30, "Pointer speed" },  { 2,   6, 72, "Clock" },
    { 2,   6, 114, "Reset" },         { 2, 156, 114, "About" }
};

static int tab;                         /* current tab */
static int hover = -1;                  /* widget under the pointer */
static int open_drop = -1;              /* dropdown whose list is open */
static int open_hover = -1;             /* list item under the pointer */

#define ITEM_H 13

/* ---------- dirty rectangles (content-relative) ---------- */
#define DQ_MAX 6
static int16_t dq[DQ_MAX][4];
static int dq_n;
static bool dq_full;

static void mark(int x, int y, int w, int h)
{
    if (dq_full) return;
    if (dq_n >= DQ_MAX) { dq_full = true; dq_n = 0; return; }
    dq[dq_n][0] = (int16_t)x;       dq[dq_n][1] = (int16_t)y;
    dq[dq_n][2] = (int16_t)(x + w); dq[dq_n][3] = (int16_t)(y + h);
    dq_n++;
}

static void mark_all(void)
{
    dq_full = true;
    dq_n = 0;
}

static void mark_widget(int id)
{
    if (id >= 0) mark(wd[id].x - 1, wd[id].y - 1, wd[id].w + 2, wd[id].h + 2);
}

/* ---------- theme ---------- */
static uint8_t darken(uint8_t v)
{
    return (uint8_t)(v * 82 / 100);
}

static void rebuild_theme(void)
{
    settings_rgb_t t;

    eff = themes[prefs.theme < THEME_COUNT ? prefs.theme : 0];
    if (prefs.wall == 1) {
        eff.bottom = eff.top;                           /* flat */
    } else if (prefs.wall == 2) {
        t = eff.top; eff.top = eff.bottom; eff.bottom = t;   /* reverse */
    }
    if (prefs.bar >= 1 && prefs.bar <= TINT_COUNT) {
        eff.bar = tints[prefs.bar - 1].c;
        eff.bar_line.r = darken(eff.bar.r);
        eff.bar_line.g = darken(eff.bar.g);
        eff.bar_line.b = darken(eff.bar.b);
    }
    if (prefs.dock >= 1 && prefs.dock <= TINT_COUNT)
        eff.dock = tints[prefs.dock - 1].c;
    if (prefs.accent >= 1 && prefs.accent <= ACCENT_COUNT)
        eff.accent = accents[prefs.accent - 1].c;
}

/* ---------- custom fonts (fontlibc font packs) ----------
 * Every AppVar that starts with "FONTPACK" is a font pack. Each pack's family name and the
 * height of each of its fonts (up to 4 per pack) become entries of the Font dropdown. Only
 * short labels are copied; nothing points into the pack after the scan. */
typedef struct {
    char    var[9];                     /* AppVar name */
    uint8_t index;                      /* font index inside the pack */
    char    label[16];
} custom_font_t;

static custom_font_t custom[MAX_CUSTOM];
static uint8_t custom_count;

#ifndef SHELL_NO_FONTLIBC
static void put_number(char *dst, unsigned v)       /* appends v to dst (v < 1000) */
{
    char tmp[4];
    int n = 0, i = (int)strlen(dst);
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 3);
    while (n) dst[i++] = tmp[--n];
    dst[i] = 0;
}

static void scan_font_packs(void)
{
    void *pos = NULL;
    const char *found;

    custom_count = 0;
    while (custom_count < MAX_CUSTOM && (found = ti_Detect(&pos, "FONTPACK")) != NULL) {
        char var[9];
        uint8_t h;
        size_t size, i;
        const uint8_t *base;
        const fontlib_font_pack_t *pack;
        char family[11];
        size_t fam_len = 0;

        strncpy(var, found, 8);
        var[8] = 0;
        h = ti_Open(var, "r");
        if (!h) continue;
        size = ti_GetSize(h);
        base = (const uint8_t *)ti_GetDataPtr(h);
        pack = (const fontlib_font_pack_t *)base;
        if (size < 16 || !pack->fontCount) { ti_Close(h); continue; }

        /* family name from the metadata when it is present and in range, else the AppVar name */
        family[0] = 0;
        if (pack->metadata > 0 && (size_t)pack->metadata + sizeof(fontlib_metadata_t) <= size) {
            const fontlib_metadata_t *meta = (const fontlib_metadata_t *)(base + pack->metadata);
            if (meta->length >= (int24_t)sizeof(fontlib_metadata_t) &&
                meta->font_family_name > 0 && (size_t)meta->font_family_name < size) {
                const char *src = (const char *)base + meta->font_family_name;
                size_t room = size - (size_t)meta->font_family_name;
                while (fam_len < sizeof(family) - 1 && fam_len < room && src[fam_len] >= ' ') {
                    family[fam_len] = src[fam_len];
                    fam_len++;
                }
                family[fam_len] = 0;
            }
        }
        if (!family[0]) strcpy(family, var);

        for (i = 0; i < pack->fontCount && i < 4 && custom_count < MAX_CUSTOM; i++) {
            custom_font_t *c = &custom[custom_count];
            int24_t off = pack->font_list[i];
            unsigned height = 0;

            if (off > 0 && (size_t)off + 16 <= size)
                height = ((const fontlib_font_t *)(base + off))->height;
            strcpy(c->var, var);
            c->index = (uint8_t)i;
            strcpy(c->label, family);
            if (height) {
                strcat(c->label, " ");
                put_number(c->label, height);
            }
            custom_count++;
        }
        ti_Close(h);
    }
}
#else
static void scan_font_packs(void) { custom_count = 0; }
#endif

/* index of the custom entry matching the saved name/index, or -1 */
static int find_custom(const char *var, uint8_t index)
{
    int k;
    for (k = 0; k < custom_count; k++)
        if (custom[k].index == index && strcmp(custom[k].var, var) == 0)
            return k;
    return -1;
}

/* ---------- storage ---------- */
static bool sane(const uint8_t *b)       /* b[1..10] = the ten settings */
{
    return b[1] < THEME_COUNT && b[2] < WALL_COUNT && b[3] <= ACCENT_COUNT &&
           b[4] <= FONT_COUNT && b[5] <= TINT_COUNT && b[6] <= TINT_COUNT &&
           b[7] < 3 && b[8] < 2 && b[9] < 2 && b[10] < 2;
}

static void load_prefs(void)
{
    uint8_t buf[STORE_SIZE];
    uint8_t h = ti_Open(STORE_NAME, "r");
    size_t n;

    prefs = defaults;
    if (h) {
        n = ti_Read(buf, 1, STORE_SIZE, h);
        if (n >= STORE_SIZE_V2 && (buf[0] == 2 || buf[0] == STORE_VERSION) && sane(buf)) {
            prefs.theme = buf[1];  prefs.wall = buf[2];  prefs.accent = buf[3];
            prefs.font = buf[4];   prefs.bar = buf[5];   prefs.dock = buf[6];
            prefs.speed = buf[7];  prefs.clock24 = buf[8];
            prefs.battery = buf[9]; prefs.tips = buf[10];
            if (buf[0] == STORE_VERSION && n == STORE_SIZE) {
                memcpy(prefs.cvar, buf + 11, 8);
                prefs.cvar[8] = 0;
                prefs.cidx = buf[20];
            }
        } else if (n >= 4 && buf[0] == 1 &&              /* old format: theme, speed, clock */
                   buf[1] < THEME_COUNT && buf[2] < 3 && buf[3] < 2) {
            prefs.theme = buf[1];
            prefs.speed = buf[2];
            prefs.clock24 = buf[3];
        }
        ti_Close(h);
    }
    if (prefs.font == FONT_CUSTOM && find_custom(prefs.cvar, prefs.cidx) < 0)
        prefs.font = FONT_DEFAULT;                  /* that font pack is no longer installed */
    rebuild_theme();
}

static void store_prefs(void)
{
    uint8_t buf[STORE_SIZE];
    uint8_t h;

    buf[0] = STORE_VERSION;
    buf[1] = prefs.theme;   buf[2] = prefs.wall;    buf[3] = prefs.accent;
    buf[4] = prefs.font;    buf[5] = prefs.bar;     buf[6] = prefs.dock;
    buf[7] = prefs.speed;   buf[8] = prefs.clock24;
    buf[9] = prefs.battery; buf[10] = prefs.tips;
    memcpy(buf + 11, prefs.cvar, 8);
    buf[19] = 0;
    buf[20] = prefs.cidx;

    h = ti_Open(STORE_NAME, "w");
    if (!h) return;
    ti_Write(buf, 1, STORE_SIZE, h);
    ti_Close(h);
    archive_pending = true;
}

void settings_init(void)
{
    scan_font_packs();
    load_prefs();
    changed = false;
    font_changed = false;
    hover = -1;
    open_drop = open_hover = -1;
    tab = 0;
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
const settings_theme_t *settings_theme(void)   { return &eff; }
uint8_t settings_pointer_speed(void)           { return prefs.speed < 3 ? prefs.speed : 1; }
bool settings_clock_24h(void)                  { return prefs.clock24 != 0; }
bool settings_show_battery(void)               { return prefs.battery != 0; }
bool settings_dock_labels(void)                { return prefs.tips != 0; }

bool settings_take_changed(void)
{
    bool c = changed;
    changed = false;
    return c;
}

bool settings_take_font_changed(void)
{
    bool c = font_changed;
    font_changed = false;
    return c;
}

/* ---------- fonts ----------
 * graphx draws text with one global font. The widths of the built-in font are captured once
 * (right after gfx_Begin), then Compact / Roomy shift them by one pixel and Mono switches to
 * a fixed 8px cell. Layouts are designed for Default; the other fonts trade some fit for looks. */
static uint8_t font_width[256];

void settings_apply_font(void)
{
    int c;

#ifdef SHELL_NO_FONTS
    return;
#endif
    gfx_SetMonospaceFont(0);
    gfx_SetFontSpacing(NULL);                       /* graphx default spacing */
    if (prefs.font == FONT_DEFAULT || prefs.font == FONT_CUSTOM) return;   /* custom: see ui_text.c */
    for (c = 0; c < 256; c++) {                     /* default widths, shifted by one pixel */
        int w = (int)gfx_GetCharWidth((char)c);
        if (prefs.font == FONT_COMPACT && w > 5) w--;
        else if (prefs.font == FONT_ROOMY && w > 0) w++;
        font_width[c] = (uint8_t)w;
    }
    gfx_SetFontSpacing(font_width);
    if (prefs.font == FONT_MONO) gfx_SetMonospaceFont(8);
}

bool settings_custom_font(char var_name[9], uint8_t *font_index)
{
    if (prefs.font != FONT_CUSTOM) return false;
    memcpy(var_name, prefs.cvar, 9);
    var_name[8] = 0;
    *font_index = prefs.cidx;
    return true;
}

/* ---------- dropdown contents ---------- */
static int drop_count(int id)
{
    switch (id) {
    case W_THEME:  return THEME_COUNT;
    case W_WALL:   return WALL_COUNT;
    case W_ACCENT: return ACCENT_COUNT + 1;
    case W_FONT:   return FONT_COUNT + custom_count;
    default:       return TINT_COUNT + 1;           /* W_BAR, W_DOCK */
    }
}

static const char *drop_item(int id, int i)
{
    switch (id) {
    case W_THEME:  return theme_names[i];
    case W_WALL:   return wall_names[i];
    case W_FONT:   return i < FONT_COUNT ? font_names[i] : custom[i - FONT_COUNT].label;
    case W_ACCENT: return i == 0 ? "Theme" : accents[i - 1].name;
    default:       return i == 0 ? "Theme" : tints[i - 1].name;
    }
}

static int drop_value(const prefs_t *p, int id)
{
    switch (id) {
    case W_THEME:  return p->theme;
    case W_WALL:   return p->wall;
    case W_ACCENT: return p->accent;
    case W_FONT: {
        int k = p->font == FONT_CUSTOM ? find_custom(p->cvar, p->cidx) : -1;
        return k >= 0 ? FONT_COUNT + k : (p->font < FONT_COUNT ? p->font : FONT_DEFAULT);
    }
    case W_BAR:    return p->bar;
    default:       return p->dock;
    }
}

static void drop_set(prefs_t *p, int id, int v)
{
    switch (id) {
    case W_THEME:  p->theme = (uint8_t)v;  break;
    case W_WALL:   p->wall = (uint8_t)v;   break;
    case W_ACCENT: p->accent = (uint8_t)v; break;
    case W_FONT:
        if (v < FONT_COUNT) {
            p->font = (uint8_t)v;
        } else {
            p->font = FONT_CUSTOM;
            strcpy(p->cvar, custom[v - FONT_COUNT].var);
            p->cidx = custom[v - FONT_COUNT].index;
        }
        break;
    case W_BAR:    p->bar = (uint8_t)v;    break;
    default:       p->dock = (uint8_t)v;   break;
    }
}

/* palette index of a color swatch for this item, or -1 for none */
static int drop_swatch(int id, int i)
{
    if (id == W_ACCENT && i > 0) return PAL_ICON0 + accents[i - 1].pal;
    return -1;
}

static const char *btn_label(int id)
{
    static const char *const names[W_COUNT] = {
        "Look", "Taskbar", "System", 0, 0, 0, 0, 0, 0,
        "Show", "Hide", "On", "Off",
        "Slow", "Normal", "Fast", "24-hour", "12-hour", "Reset all"
    };
    return names[id];
}

static bool btn_sel(const prefs_t *p, int id)
{
    switch (id) {
    case W_TAB0: case W_TAB1: case W_TAB2: return tab == id - W_TAB0;
    case W_BATT_ON:  return p->battery != 0;
    case W_BATT_OFF: return p->battery == 0;
    case W_TIPS_ON:  return p->tips != 0;
    case W_TIPS_OFF: return p->tips == 0;
    case W_SPEED0: case W_SPEED1: case W_SPEED2: return p->speed == id - W_SPEED0;
    case W_CLK24: return p->clock24 != 0;
    case W_CLK12: return p->clock24 == 0;
    default: return false;
    }
}

/* ---------- hit testing (content-relative coordinates) ---------- */
static bool visible(int id)
{
    return wd[id].tab == TAB_ALL || wd[id].tab == tab;
}

static int widget_at(int x, int y)
{
    int id;
    for (id = 0; id < W_COUNT; id++) {
        if (!visible(id)) continue;
        if (x >= wd[id].x && x < wd[id].x + wd[id].w &&
            y >= wd[id].y && y < wd[id].y + wd[id].h)
            return id;
    }
    return -1;
}

typedef struct { int x, y, w, h, n; } dd_list_t;

static dd_list_t list_geom(int id)
{
    dd_list_t l;
    l.n = drop_count(id);
    l.h = l.n * ITEM_H + 6;
    l.x = wd[id].x;
    l.w = wd[id].w;
    l.y = wd[id].y;                         /* pops up over its button, like a menu */
    if (l.y + l.h > CONTENT_H - 2) l.y = CONTENT_H - 2 - l.h;
    if (l.y < 2) l.y = 2;
    return l;
}

static bool in_list(int x, int y)
{
    dd_list_t l;
    if (open_drop < 0) return false;
    l = list_geom(open_drop);
    return x >= l.x && x < l.x + l.w && y >= l.y && y < l.y + l.h;
}

static int list_item_at(int x, int y)
{
    dd_list_t l;
    if (open_drop < 0) return -1;
    l = list_geom(open_drop);
    if (x < l.x || x >= l.x + l.w || y < l.y + 3 || y >= l.y + 3 + l.n * ITEM_H) return -1;
    return (y - l.y - 3) / ITEM_H;
}

static void mark_item(int i)
{
    dd_list_t l;
    if (open_drop < 0 || i < 0) return;
    l = list_geom(open_drop);
    mark(l.x, l.y + 3 + i * ITEM_H, l.w, ITEM_H);
}

/* ---------- drawing ---------- */
static void text_color(uint8_t fg)
{
    ui_text_color(fg);
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

static bool touches(int ax0, int ay0, int ax1, int ay1,
                    int bx0, int by0, int bx1, int by1)
{
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

static void draw_button(int x, int y, int id)
{
    const widget_t *r = &wd[id];
    const char *s = btn_label(id);
    bool sel = btn_sel(&prefs, id);
    bool hov = id == hover;
    uint8_t bg, fg;

    if (id == W_RESET) {
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
    fill_rr(x + r->x, y + r->y, r->w, r->h, 6);
    text_color(fg);
    ui_print(s, x + r->x + (r->w - (int)ui_text_width(s)) / 2,
                         y + r->y + (r->h - 8) / 2);
}

static void draw_dropdown(int x, int y, int id)
{
    const widget_t *r = &wd[id];
    int v = drop_value(&prefs, id);
    int sw = drop_swatch(id, v);
    int tx = x + r->x + 8;
    bool open = id == open_drop;

    if (open) {                                     /* accent ring while open */
        gfx_SetColor(PAL_ACCENT);
        fill_rr(x + r->x - 1, y + r->y - 1, r->w + 2, r->h + 2, 7);
    }
    gfx_SetColor((id == hover || open) ? PAL_LABEL : PAL_BAR);
    fill_rr(x + r->x, y + r->y, r->w, r->h, 6);

    if (sw >= 0) {
        gfx_SetColor((uint8_t)sw);
        gfx_FillRectangle(tx, y + r->y + 5, 8, 8);
        tx += 12;
    }
    text_color(PAL_TEXT);
    ui_print(drop_item(id, v), tx, y + r->y + (r->h - 8) / 2);

    gfx_SetColor(PAL_TEXT);                         /* chevron */
    gfx_FillTriangle(x + r->x + r->w - 14, y + r->y + 7,
                     x + r->x + r->w - 6,  y + r->y + 7,
                     x + r->x + r->w - 10, y + r->y + 11);
}

static void draw_list(int x, int y)
{
    dd_list_t l = list_geom(open_drop);
    int cur = drop_value(&prefs, open_drop);
    int i;

    gfx_SetColor(PAL_TEXT);                         /* outline */
    fill_rr(x + l.x - 1, y + l.y - 1, l.w + 2, l.h + 2, 6);
    gfx_SetColor(PAL_LABEL);
    fill_rr(x + l.x, y + l.y, l.w, l.h, 5);

    for (i = 0; i < l.n; i++) {
        int iy = y + l.y + 3 + i * ITEM_H;
        int sw = drop_swatch(open_drop, i);
        int tx = x + l.x + 16;
        bool hot = i == open_hover;

        if (hot) {
            gfx_SetColor(PAL_ACCENT);
            gfx_FillRectangle(x + l.x + 3, iy, l.w - 6, ITEM_H);
        }
        if (i == cur) {                             /* current value */
            gfx_SetColor(hot ? PAL_LABEL : PAL_ACCENT);
            gfx_FillCircle(x + l.x + 9, iy + ITEM_H / 2, 2);
        }
        if (sw >= 0) {
            gfx_SetColor((uint8_t)sw);
            gfx_FillRectangle(tx, iy + 2, 8, 8);
            tx += 12;
        }
        text_color(hot ? PAL_LABEL : PAL_TEXT);
        ui_print(drop_item(open_drop, i), tx, iy + (ITEM_H - 8) / 2);
    }
}

static void draw_look_preview(int x, int y)
{
    gfx_SetColor(PAL_LABEL);
    fill_rr(x + 6, y + 122, 292, 40, 6);
    text_color(PAL_TEXT);
    ui_print("atmOS  Aa Bb Cc  0123456789", x + 14, y + 130);
    ui_print("The quick brown fox jumps over", x + 14, y + 144);
}

/* a miniature of the real screen: menu bar, wallpaper strip, dock */
static void draw_taskbar_preview(int x, int y)
{
    int i;

    gfx_SetColor(PAL_BAR);
    gfx_FillRectangle(x + 6, y + 120, 292, 13);
    gfx_SetColor(PAL_BAR_LINE);
    gfx_HorizLine(x + 6, y + 133, 292);
    text_color(PAL_TEXT);
    ui_print("atmOS", x + 12, y + 123);
    ui_print("12:00", x + 262, y + 123);

    for (i = 0; i < 4; i++) {                       /* wallpaper */
        gfx_SetColor(PAL_GRAD0 + i * 5);
        gfx_FillRectangle(x + 6, y + 134 + i * 6, 292, 6);
    }

    gfx_SetColor(PAL_DOCK);
    gfx_FillRectangle(x + 6, y + 158, 292, 14);
    for (i = 0; i < 3; i++) {
        gfx_SetColor(PAL_ICON0 + (i == 1 ? 3 : i == 0 ? 2 : 1));
        gfx_FillRectangle(x + 125 + i * 20, y + 161, 10, 8);
    }
    gfx_SetColor(PAL_ACCENT);
    gfx_Rectangle(x + 143 + 2, y + 160, 14, 10);
}

/* draws only what overlaps [x0,x1) x [y0,y1) (screen coordinates) */
void settings_draw_region(int x, int y, int x0, int y0, int x1, int y1)
{
    NOTE_STACK();
    int id;
    size_t i;

    gfx_SetTextScale(1, 1);

    for (id = 0; id < W_COUNT; id++) {
        if (!visible(id)) continue;
        if (!touches(x0, y0, x1, y1, x + wd[id].x - 1, y + wd[id].y - 1,
                     x + wd[id].x + wd[id].w + 1, y + wd[id].y + wd[id].h + 1))
            continue;
        if (IS_DROP(id)) draw_dropdown(x, y, id);
        else             draw_button(x, y, id);
    }

    for (i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
        if (labels[i].tab != tab) continue;
        if (!touches(x0, y0, x1, y1, x + labels[i].x, y + labels[i].y,
                     x + WIN_W, y + labels[i].y + 9))
            continue;
        text_color(PAL_TEXT);
        ui_print(labels[i].s, x + labels[i].x, y + labels[i].y);
    }

    if (tab == 0 && touches(x0, y0, x1, y1, x + 6, y + 122, x + 298, y + 162))
        draw_look_preview(x, y);
    if (tab == 1 && touches(x0, y0, x1, y1, x + 6, y + 120, x + 298, y + 172))
        draw_taskbar_preview(x, y);
    if (tab == 2 && touches(x0, y0, x1, y1, x + 156, y + 126, x + 298, y + 160)) {
        text_color(PAL_TEXT);
        ui_print("atmOS for TI-84 Plus CE", x + 156, y + 127);
        ui_print("Saved automatically", x + 156, y + 141);
    }

    if (touches(x0, y0, x1, y1, x, y + CONTENT_H - 10, x + WIN_W, y + CONTENT_H - 2) &&
        open_drop < 0) {
        text_color(PAL_ICON0 + 7);
        ui_print("Changes apply right away.", x + 6, y + CONTENT_H - 10);
    }

    if (open_drop >= 0) {
        dd_list_t l = list_geom(open_drop);
        if (touches(x0, y0, x1, y1, x + l.x - 1, y + l.y - 1,
                    x + l.x + l.w + 1, y + l.y + l.h + 1))
            draw_list(x, y);
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
    open_drop = open_hover = -1;
    dq_n = 0;
    dq_full = false;
}

bool settings_hover(int x, int y)
{
    if (open_drop >= 0) {                           /* an open list captures the pointer */
        int it = list_item_at(x, y);
        if (it == open_hover) return false;
        mark_item(open_hover);
        mark_item(it);
        open_hover = it;
        return true;
    }
    {
        int id = widget_at(x, y);
        if (id == hover) return false;
        mark_widget(hover);                         /* un-highlight the old one */
        mark_widget(id);                            /* highlight the new one */
        hover = id;
        return true;
    }
}

int settings_dirty(int x, int y, int r[][4], int max)
{
    int i, n = dq_n;

    if (dq_full || n > max) {
        dq_full = false;
        dq_n = 0;
        return -1;                                  /* too many pieces: repaint the window */
    }
    for (i = 0; i < n; i++) {
        r[i][0] = x + dq[i][0];
        r[i][1] = y + dq[i][1];
        r[i][2] = x + dq[i][2];
        r[i][3] = y + dq[i][3];
    }
    dq_n = 0;
    return n;
}

static void apply(const prefs_t *next)
{
    prefs_t old = prefs;
    bool font_diff, look_diff;
    int id;

    if (memcmp(&old, next, sizeof(old)) == 0) return;   /* nothing changed: skip the flash write */
    prefs = *next;
    rebuild_theme();

    font_diff = old.font != next->font || old.cidx != next->cidx ||
                strcmp(old.cvar, next->cvar) != 0;
    look_diff = font_diff || old.theme != next->theme || old.wall != next->wall ||
                old.accent != next->accent || old.bar != next->bar || old.dock != next->dock;
    if (font_diff) font_changed = true;

    if (look_diff) {
        mark_all();                                 /* colors / text widths changed everywhere */
    } else {
        for (id = 0; id < W_COUNT; id++)            /* only buttons whose selection flipped */
            if (visible(id) && !IS_DROP(id) && btn_sel(&old, id) != btn_sel(&prefs, id))
                mark_widget(id);
    }
    changed = true;
    store_prefs();
}

bool settings_cancel(void)
{
    if (open_drop < 0) return false;
    open_drop = open_hover = -1;
    mark_all();
    return true;
}

void settings_click(int x, int y)
{
    int id;
    prefs_t next = prefs;

    if (open_drop >= 0) {
        int it = list_item_at(x, y);
        int w = open_drop;
        if (it < 0 && in_list(x, y)) return;        /* padding inside the list: ignore */
        open_drop = open_hover = -1;
        mark_all();
        if (it >= 0) {
            drop_set(&next, w, it);
            apply(&next);
        }
        return;                                     /* a click outside just closes it */
    }

    id = widget_at(x, y);
    if (id < 0) return;

    if (id <= W_TAB2) {
        if (tab != id - W_TAB0) {
            tab = id - W_TAB0;
            hover = -1;
            mark_all();
        }
        return;
    }
    if (IS_DROP(id)) {
        open_drop = id;
        open_hover = drop_value(&prefs, id);
        mark_all();
        return;
    }

    switch (id) {
    case W_BATT_ON:  next.battery = 1; break;
    case W_BATT_OFF: next.battery = 0; break;
    case W_TIPS_ON:  next.tips = 1;    break;
    case W_TIPS_OFF: next.tips = 0;    break;
    case W_SPEED0: case W_SPEED1: case W_SPEED2:
        next.speed = (uint8_t)(id - W_SPEED0);
        break;
    case W_CLK24: next.clock24 = 1; break;
    case W_CLK12: next.clock24 = 0; break;
    case W_RESET: next = defaults;  break;
    default: return;
    }
    apply(&next);
}

bool settings_keys(void)
{
    return false;                       /* pointer-first: arrows move the pointer */
}

bool settings_press(void)
{
    return false;                       /* Enter / 2nd click the hovered widget */
}