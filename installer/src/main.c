/*
 * atmOS Installer - first-time setup mirrors the shell's Settings app.
 *
 * The settings, their choices and the SHLSET layout (version 3, 21 bytes) are
 * the same as the shell's settings.c, so what the user picks here is exactly
 * what atmOS loads on its first start:
 *   0 version  1 theme  2 wallpaper  3 accent  4 font (0 default / 4 custom)
 *   5 menu bar tint  6 dock tint  7 pointer speed  8 clock24  9 battery
 *   10 dock labels  11..18 custom font pack AppVar name  19 = 0  20 font index
 *
 * fileioc (SHLSET, font pack scan) is only used while GraphX is off.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <graphx.h>
#include <fontlibc.h>
#include <keypadc.h>
#include <fileioc.h>
#include <ti/vars.h>
#include <ti/getcsc.h>
#include <ti/screen.h>
#include <sys/timers.h>

#include "config.h"
#include "app.h"
#include "flash.h"
#include "ports.h"

#define MAX_APPVARS 32
#define STORE_NAME     "SHLSET"
#define STORE_VERSION  3
#define STORE_SIZE     21
#define STORE_SIZE_V2  11

/* same limits as settings.c */
#define THEME_COUNT        6
#define MOD_THEME_MAX      4      /* SETTINGS_MOD_THEME_MAX in the shell */
#define WALL_COUNT         3
#define TINT_COUNT         7
#define ACCENT_COUNT       7
#define FONT_CUSTOM        4
#define MAX_CUSTOM         8

enum {
    SUCCESS,
    MISSING_VAR,
    PORT_SETUP_FAILED,
    NO_SPACE,
    USER_ABORT
};

struct appvar {
    uint8_t *data;
    uint24_t size;
    uintptr_t app_offset;
};

static struct appvar appvars[MAX_APPVARS];
static char app_name[10];

/* ---------- the shell's choices (copied from settings.c) ---------- */
typedef struct { uint8_t r, g, b; } rgb_t;
typedef struct { rgb_t top, bottom, bar, bar_line, dock, accent; } theme_t;

static const char *const theme_names[THEME_COUNT] = {
    "Sunset", "Ocean", "Forest", "Grape", "Mono", "Candy"
};

static const theme_t themes[THEME_COUNT] = {
    { {  60, 100, 190 }, { 200, 120, 170 }, { 236, 236, 240 }, { 190, 190, 198 },
      { 225, 225, 235 }, {   0, 122, 255 } },
    { {  10,  60, 120 }, {  70, 190, 210 }, { 232, 240, 244 }, { 176, 196, 206 },
      { 220, 232, 238 }, {   0, 150, 199 } },
    { {  24,  70,  52 }, { 150, 190, 110 }, { 236, 240, 232 }, { 184, 196, 176 },
      { 226, 232, 220 }, {  40, 160,  80 } },
    { {  60,  30, 110 }, { 220, 110, 170 }, { 240, 234, 244 }, { 196, 184, 206 },
      { 232, 224, 238 }, { 150,  70, 220 } },
    { {  40,  40,  46 }, { 150, 150, 160 }, { 238, 238, 240 }, { 190, 190, 194 },
      { 226, 226, 230 }, { 100, 110, 130 } },
    { { 250, 120, 160 }, { 255, 210, 120 }, { 250, 238, 240 }, { 206, 186, 192 },
      { 244, 228, 232 }, { 240,  70, 120 } }
};

static const char *const wall_names[WALL_COUNT] = { "Gradient", "Flat", "Reverse" };

static const struct { const char *name; rgb_t c; } tints[TINT_COUNT] = {
    { "White",    { 248, 248, 250 } }, { "Silver",   { 222, 222, 228 } },
    { "Cream",    { 250, 244, 226 } }, { "Mint",     { 220, 242, 230 } },
    { "Sky",      { 218, 234, 250 } }, { "Rose",     { 250, 226, 232 } },
    { "Lavender", { 236, 228, 250 } }
};

static const struct { const char *name; rgb_t c; } accents[ACCENT_COUNT] = {
    { "Red",    { 255,  95,  86 } }, { "Orange", { 255, 159,  10 } },
    { "Green",  {  39, 201,  63 } }, { "Blue",   {  10, 132, 255 } },
    { "Purple", { 191,  90, 242 } }, { "Cyan",   { 100, 210, 255 } },
    { "Gray",   { 142, 142, 147 } }
};

static const char *const speed_names[3] = { "Slow", "Normal", "Fast" };

/* ---------- chosen values (defaults = the shell's defaults) ---------- */
static struct {
    uint8_t theme, wall, accent, bar, dock;
    uint8_t speed, clock24, battery, tips;
    uint8_t font_sel;                   /* 0 = Default, n = custom[n - 1] */
} pf = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 0 };

/* ---------- custom fonts: every AppVar starting with "FONTPACK" (as in the shell) ---------- */
typedef struct {
    char    var[9];
    uint8_t index;
    char    label[16];
} custom_font_t;

static custom_font_t custom[MAX_CUSTOM];
static uint8_t custom_count;

static void put_number(char *dst, unsigned v)
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

/* ---------- SHLSET (call only when GraphX is off) ---------- */
static bool sane(const uint8_t *b)
{
    return b[1] < THEME_COUNT + MOD_THEME_MAX && b[2] < WALL_COUNT && b[3] <= ACCENT_COUNT &&
           b[4] <= FONT_CUSTOM && b[5] <= TINT_COUNT && b[6] <= TINT_COUNT &&
           b[7] < 3 && b[8] < 2 && b[9] < 2 && b[10] < 2;
}

/* True when a valid SHLSET already exists (= first-time setup was done before). */
static bool load_prefs(void)
{
    uint8_t buf[STORE_SIZE];
    uint8_t h = ti_Open(STORE_NAME, "r");
    size_t n;

    if (!h) return false;
    n = ti_Read(buf, 1, STORE_SIZE, h);
    ti_Close(h);
    return n >= STORE_SIZE_V2 && (buf[0] == 2 || buf[0] == STORE_VERSION) && sane(buf);
}

static void save_prefs(void)
{
    uint8_t buf[STORE_SIZE];
    uint8_t h;

    memset(buf, 0, sizeof buf);
    buf[0] = STORE_VERSION;
    buf[1] = pf.theme;   buf[2] = pf.wall;    buf[3] = pf.accent;
    buf[4] = pf.font_sel ? FONT_CUSTOM : 0;
    buf[5] = pf.bar;     buf[6] = pf.dock;
    buf[7] = pf.speed;   buf[8] = pf.clock24;
    buf[9] = pf.battery; buf[10] = pf.tips;
    if (pf.font_sel) {
        memcpy(buf + 11, custom[pf.font_sel - 1].var, 8);
        buf[20] = custom[pf.font_sel - 1].index;
    }
    h = ti_Open(STORE_NAME, "w");
    if (!h) return;
    ti_Write(buf, 1, STORE_SIZE, h);
    ti_SetArchiveStatus(true, h);
    ti_Close(h);
}

/* ---------- palette ---------- */
enum {
    PAL_GRAD0 = 0,
    PAL_BAR = 16, PAL_BAR_LINE, PAL_DOCK,
    PAL_TEXT, PAL_LABEL, PAL_ACCENT,
    PAL_RED, PAL_GREEN, PAL_GRAY
};

static void set_pal(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    gfx_palette[idx] = gfx_RGBTo1555(r, g, b);
}

static uint8_t darken(uint8_t v)
{
    return (uint8_t)(v * 82 / 100);
}

/* Same rules as the shell's rebuild_theme(): preset, wallpaper style, tints, accent. */
static void apply_theme(void)
{
    theme_t t = themes[pf.theme < THEME_COUNT ? pf.theme : 0];
    rgb_t tmp;
    int i;

    if (pf.wall == 1) {
        t.bottom = t.top;
    } else if (pf.wall == 2) {
        tmp = t.top; t.top = t.bottom; t.bottom = tmp;
    }
    if (pf.bar >= 1 && pf.bar <= TINT_COUNT) {
        t.bar = tints[pf.bar - 1].c;
        t.bar_line.r = darken(t.bar.r);
        t.bar_line.g = darken(t.bar.g);
        t.bar_line.b = darken(t.bar.b);
    }
    if (pf.dock >= 1 && pf.dock <= TINT_COUNT)
        t.dock = tints[pf.dock - 1].c;
    if (pf.accent >= 1 && pf.accent <= ACCENT_COUNT)
        t.accent = accents[pf.accent - 1].c;

    for (i = 0; i < 16; i++) {
        set_pal(PAL_GRAD0 + i,
            t.top.r + ((int)t.bottom.r - (int)t.top.r) * i / 15,
            t.top.g + ((int)t.bottom.g - (int)t.top.g) * i / 15,
            t.top.b + ((int)t.bottom.b - (int)t.top.b) * i / 15);
    }
    set_pal(PAL_BAR,      t.bar.r,      t.bar.g,      t.bar.b);
    set_pal(PAL_BAR_LINE, t.bar_line.r, t.bar_line.g, t.bar_line.b);
    set_pal(PAL_DOCK,     t.dock.r,     t.dock.g,     t.dock.b);
    set_pal(PAL_TEXT,      30,  30,  34);
    set_pal(PAL_LABEL,    255, 255, 255);
    set_pal(PAL_ACCENT,   t.accent.r,   t.accent.g,   t.accent.b);
    set_pal(PAL_RED,      255,  95,  86);
    set_pal(PAL_GREEN,     39, 201,  63);
    set_pal(PAL_GRAY,     142, 142, 147);
}

/* ---------- drawing ---------- */
static void fill_round(int x, int y, int w, int h, int r)
{
    gfx_FillRectangle(x + r, y, w - 2 * r, h);
    gfx_FillRectangle(x, y + r, w, h - 2 * r);
    gfx_FillCircle(x + r, y + r, r);
    gfx_FillCircle(x + w - r - 1, y + r, r);
    gfx_FillCircle(x + r, y + h - r - 1, r);
    gfx_FillCircle(x + w - r - 1, y + h - r - 1, r);
}

static void draw_wallpaper(void)
{
    int band = GFX_LCD_HEIGHT / 16;
    for (int i = 0; i < 16; i++) {
        gfx_SetColor(PAL_GRAD0 + i);
        gfx_FillRectangle(0, i * band, GFX_LCD_WIDTH,
            (i == 15) ? GFX_LCD_HEIGHT - i * band : band);
    }
}

static void draw_panel(int x, int y, int w, int h)
{
    gfx_SetColor(PAL_BAR);
    fill_round(x, y, w, h, 10);
    gfx_SetColor(PAL_BAR_LINE);
    gfx_Rectangle(x, y, w, h);
}

static void draw_button(int x, int y, int w, int h, const char *label, bool selected)
{
    gfx_SetColor(selected ? PAL_ACCENT : PAL_DOCK);
    fill_round(x, y, w, h, 8);
    gfx_SetTextFGColor(selected ? PAL_LABEL : PAL_TEXT);
    gfx_SetTextBGColor(255);
    gfx_SetTextTransparentColor(255);
    int tw = gfx_GetStringWidth(label);
    gfx_PrintStringXY(label, x + (w - tw) / 2, y + (h - 8) / 2);
}

static void center_text(const char *s, int y, uint8_t color)
{
    gfx_SetTextFGColor(color);
    gfx_SetTextBGColor(255);
    gfx_SetTextTransparentColor(255);
    int tw = gfx_GetStringWidth(s);
    gfx_PrintStringXY(s, (GFX_LCD_WIDTH - tw) / 2, y);
}

/* Preview line in the chosen font (fontlib), set up the way the shell's ui_text.c does it. */
static void preview_font(const char *s, int y)
{
    if (pf.font_sel == 0) {
        center_text(s, y, PAL_TEXT);
        return;
    }
    fontlib_font_t *f = fontlib_GetFontByIndex(custom[pf.font_sel - 1].var,
                                               custom[pf.font_sel - 1].index);
    if (!f || !fontlib_SetFont(f, 0)) {
        center_text(s, y, PAL_TEXT);
        return;
    }
    fontlib_SetLineSpacing(0, 0);
    fontlib_SetTransparency(true);
    fontlib_SetNewlineOptions(0);
    fontlib_SetWindowFullScreen();
    fontlib_SetColors(PAL_TEXT, 255);
    fontlib_SetCursorPosition((GFX_LCD_WIDTH - fontlib_GetStringWidth(s)) / 2, y);
    fontlib_DrawString(s);
}

/* ---------- install ---------- */
static void *pmax(void *a, void *b) { return a > b ? a : b; }
static void *pmin(void *a, void *b) { return a < b ? a : b; }

static bool check_variable_overlaps(const uint8_t *app_start)
{
    uint8_t first_erased_page = ADDR_TO_PAGE(app_start) - 1;
    void *entry = os_GetSymTablePtr();
    uint24_t type, length;
    char name[9];
    void *data;

    while ((entry = os_NextSymEntry(entry, &type, &length, name, &data)) != NULL) {
        if (data >= (void *)os_RamStart) continue;
        const uint8_t page = ADDR_TO_PAGE(data);
        if (page < first_erased_page) continue;
        bool ours = false;
        for (uint8_t i = 0; i < MAX_APPVARS; ++i) {
            char appvar_name[10];
            sprintf(appvar_name, APPVAR_PREFIX "%u", i);
            if (strncmp(name, appvar_name, length) == 0) {
                const uint24_t addr = (uintptr_t)app_start + (APPVAR_SPLIT_SIZE * i);
                if (page >= ADDR_TO_PAGE(addr)) return false;
                ours = true;
                break;
            }
        }
        if (!ours) return false;
    }
    return true;
}

static bool appvar_exists(uint8_t i)
{
    char name[10];
    sprintf(name, APPVAR_PREFIX "%u", i);
    return os_ChkFindSym(OS_TYPE_APPVAR, name, NULL, NULL);
}

static int do_install(void)
{
    uint24_t app_size = 0;
    if (port_setup()) return PORT_SETUP_FAILED;

    struct appvar *appvar = &appvars[0];
    for (uint8_t i = 0; i < MAX_APPVARS; ++i) {
        char name[10];
        void *data;
        const int namelen = sprintf(name, APPVAR_PREFIX "%u", i);
        if (os_ChkFindSym(OS_TYPE_APPVAR, name, NULL, &data)) {
            uint8_t *d = data;
            if (d < os_RamStart) d += 10 + namelen;
            appvar->size = *(uint16_t *)d;
            appvar->data = d + 2;
            appvar->app_offset = app_size;
            if (i == 0) {
                strncpy(app_name, (const char *)(appvar->data + APP_NAME_OFFSET), sizeof app_name);
                app_name[sizeof(app_name) - 1] = 0;
            }
            app_size += appvar->size;
            /* The last chunk is the short one, or the one with no successor. The size can
             * be a few bytes off the -m split value (65215 vs 65200), so a chunk within
             * 50 bytes of it still counts as a full one. */
            if (appvar->size + 50 < APPVAR_SPLIT_SIZE || !appvar_exists(i + 1)) break;
            appvar++;
        } else {
            return MISSING_VAR;
        }
    }
    appvar->size += sizeof(uint24_t);
    uint8_t *app_end = find_last_app();
    uint8_t *app_start = app_end - app_size - sizeof(uint24_t);
    if (!check_variable_overlaps(app_start)) return NO_SPACE;

    gfx_Begin();
    apply_theme();
    gfx_SetDrawBuffer();
    draw_wallpaper();
    draw_panel(20, 40, 280, 160);
    center_text("Installing atmOS...", 70, PAL_TEXT);
    center_text(app_name, 95, PAL_ACCENT);
    center_text("Please wait - do not reset", 130, PAL_GRAY);
    gfx_SwapDraw();
    delay(600);
    gfx_End();

    void *written = app_end;
    bool wrote_size = false;
    for (uint8_t page = ADDR_TO_PAGE(app_end);
         page >= ADDR_TO_PAGE(app_start) && appvar >= &appvars[0]; page--) {
        while (written > PAGE_TO_ADDR(page) && appvar >= &appvars[0]) {
            uint8_t *seg_start = app_start + appvar->app_offset;
            uint8_t *dst_end = pmin(seg_start + appvar->size, PAGE_TO_ADDR(page + 1));
            uint8_t *src_start = appvar->data;
            uint8_t *dst_start = pmax(seg_start, PAGE_TO_ADDR(page));
            src_start += dst_start - seg_start;
            if (!wrote_size) {
                flash_write(dst_start, src_start, (dst_end - dst_start) - sizeof(uint24_t));
                flash_write(dst_end - sizeof(uint24_t), &app_size, sizeof(uint24_t));
                wrote_size = true;
            } else {
                flash_write(dst_start, src_start, (dst_end - dst_start));
            }
            written = dst_start;
            if (dst_start == seg_start) appvar--;
        }
        flash_erase(page - 1);
    }
    fix_relocations(app_start);
    return SUCCESS;
}

/* Wait for a key press AND its release, then flush the OS key buffer so the
 * home screen never sees the Enter that dismissed us (that re-ran the program). */
static bool wait_key(void)
{
    while (kb_AnyKey()) kb_Scan();
    while (!kb_AnyKey()) kb_Scan();
    while (kb_AnyKey()) kb_Scan();
    while (os_GetCSC()) {}
    return true;
}

/* ---------- screens ---------- */
static int screen_welcome(void)
{
    int sel = 1, prev = -1;
    while (kb_AnyKey()) kb_Scan();
    while (1) {
        if (sel != prev) {
            apply_theme();
            draw_wallpaper();
            draw_panel(16, 30, 288, 180);
            center_text("atmOS Installer", 50, PAL_TEXT);
            center_text("First-time setup", 70, PAL_GRAY);
            gfx_SetTextFGColor(PAL_TEXT);
            gfx_PrintStringXY("This will install or replace", 40, 100);
            gfx_PrintStringXY("the atmOS shell on your", 40, 114);
            gfx_PrintStringXY("TI-84 Plus CE.", 40, 128);
            draw_button(60, 160, 90, 28, "Cancel", sel == 0);
            draw_button(170, 160, 90, 28, "Continue", sel == 1);
            center_text("Arrows = choose   Enter = OK", 200, PAL_GRAY);
            gfx_SwapDraw();
            prev = sel;
        }
        kb_Scan();
        if (kb_Data[7] & kb_Left)  sel = 0;
        if (kb_Data[7] & kb_Right) sel = 1;
        if (kb_Data[6] & kb_Clear) return USER_ABORT;
        if (kb_Data[6] & kb_Enter) {
            while (kb_AnyKey()) kb_Scan();
            return (sel == 1) ? SUCCESS : USER_ABORT;
        }
    }
}

/* One row per shell setting, in the order of the shell's Settings tabs. */
enum {
    R_THEME, R_WALL, R_ACCENT, R_FONT,          /* Look */
    R_BAR, R_DOCK, R_BATT, R_TIPS,              /* Taskbar */
    R_SPEED, R_CLOCK,                           /* System */
    R_CONTINUE,
    R_COUNT
};

#define VISIBLE_ROWS 6

static void row_text(int r, char *out)
{
    switch (r) {
        case R_THEME:  sprintf(out, "Theme: %s", theme_names[pf.theme]); break;
        case R_WALL:   sprintf(out, "Wallpaper: %s", wall_names[pf.wall]); break;
        case R_ACCENT: sprintf(out, "Accent color: %s",
                               pf.accent ? accents[pf.accent - 1].name : "Theme"); break;
        case R_FONT:   sprintf(out, "Font: %s",
                               pf.font_sel ? custom[pf.font_sel - 1].label : "Default"); break;
        case R_BAR:    sprintf(out, "Menu bar color: %s",
                               pf.bar ? tints[pf.bar - 1].name : "Theme"); break;
        case R_DOCK:   sprintf(out, "Dock color: %s",
                               pf.dock ? tints[pf.dock - 1].name : "Theme"); break;
        case R_BATT:   sprintf(out, "Battery indicator: %s", pf.battery ? "On" : "Off"); break;
        case R_TIPS:   sprintf(out, "Dock labels: %s", pf.tips ? "On" : "Off"); break;
        case R_SPEED:  sprintf(out, "Pointer speed: %s", speed_names[pf.speed]); break;
        case R_CLOCK:  sprintf(out, "Clock: %s", pf.clock24 ? "24-hour" : "12-hour"); break;
        default:       strcpy(out, "Continue"); break;
    }
}

static uint8_t cycle(uint8_t v, int n, int d)
{
    return (uint8_t)((v + n + d) % n);
}

static void row_change(int r, int d)
{
    switch (r) {
        case R_THEME:  pf.theme    = cycle(pf.theme,  THEME_COUNT, d);      break;
        case R_WALL:   pf.wall     = cycle(pf.wall,   WALL_COUNT, d);       break;
        case R_ACCENT: pf.accent   = cycle(pf.accent, ACCENT_COUNT + 1, d); break;
        case R_FONT:   pf.font_sel = cycle(pf.font_sel, custom_count + 1, d); break;
        case R_BAR:    pf.bar      = cycle(pf.bar,    TINT_COUNT + 1, d);   break;
        case R_DOCK:   pf.dock     = cycle(pf.dock,   TINT_COUNT + 1, d);   break;
        case R_BATT:   pf.battery  = cycle(pf.battery, 2, d);               break;
        case R_TIPS:   pf.tips     = cycle(pf.tips,   2, d);                break;
        case R_SPEED:  pf.speed    = cycle(pf.speed,  3, d);                break;
        case R_CLOCK:  pf.clock24  = cycle(pf.clock24, 2, d);               break;
        default: break;
    }
}

static int screen_customize(void)
{
    int sel = 0, top = 0;
    bool dirty = true;
    char text[40];
    while (kb_AnyKey()) kb_Scan();

    while (1) {
        if (dirty) {
            if (sel < top) top = sel;
            if (sel >= top + VISIBLE_ROWS) top = sel - VISIBLE_ROWS + 1;

            apply_theme();
            draw_wallpaper();
            draw_panel(12, 10, 296, 220);
            sprintf(text, "Customize  (%d/%d)", sel + 1, R_COUNT);
            center_text(text, 18, PAL_TEXT);
            for (int i = 0; i < VISIBLE_ROWS; i++) {
                int r = top + i;
                row_text(r, text);
                draw_button(40, 36 + i * 24, 240, 22, text, sel == r);
            }
            preview_font("Aa Bb 123 atmOS", 186);
            center_text("Up/Down move  Left/Right change", 214, PAL_GRAY);
            gfx_SwapDraw();
            dirty = false;
        }

        kb_Scan();
        if (kb_Data[7] & kb_Up)   { if (sel > 0) sel--; dirty = true; while (kb_AnyKey()) kb_Scan(); }
        if (kb_Data[7] & kb_Down) { if (sel < R_COUNT - 1) sel++; dirty = true; while (kb_AnyKey()) kb_Scan(); }
        if (kb_Data[6] & kb_Clear) return USER_ABORT;

        if (sel != R_CONTINUE && (kb_Data[7] & (kb_Left | kb_Right))) {
            row_change(sel, (kb_Data[7] & kb_Right) ? 1 : -1);
            dirty = true;
            while (kb_AnyKey()) kb_Scan();
        }
        if ((kb_Data[6] & kb_Enter) && sel == R_CONTINUE) {
            while (kb_AnyKey()) kb_Scan();
            return SUCCESS;
        }
    }
}

static int screen_options(bool *delete_after)
{
    *delete_after = true;
    int sel = 1, prev = -1;
    while (kb_AnyKey()) kb_Scan();
    while (1) {
        if (sel != prev) {
            apply_theme();
            draw_wallpaper();
            draw_panel(16, 30, 288, 180);
            center_text("Options", 48, PAL_TEXT);
            center_text("After successful install:", 70, PAL_GRAY);
            draw_button(40, 100, 240, 26, "Keep installer + AppVars", sel == 0);
            draw_button(40, 136, 240, 26, "Delete installer + AppVars", sel == 1);
            center_text("Arrows = choose  Enter = OK", 180, PAL_GRAY);
            gfx_SwapDraw();
            prev = sel;
        }
        kb_Scan();
        if (kb_Data[7] & kb_Up)   sel = 0;
        if (kb_Data[7] & kb_Down) sel = 1;
        if (kb_Data[6] & kb_Clear) return USER_ABORT;
        if (kb_Data[6] & kb_Enter) {
            *delete_after = (sel == 1);
            while (kb_AnyKey()) kb_Scan();
            return SUCCESS;
        }
    }
}

static void screen_result(int error, bool delete_after)
{
    gfx_Begin();
    apply_theme();
    gfx_SetDrawBuffer();
    draw_wallpaper();
    draw_panel(16, 40, 288, 160);
    if (error == SUCCESS) {
        center_text("Successfully installed!", 70, PAL_GREEN);
        center_text(app_name, 90, PAL_ACCENT);
        if (delete_after) {
            for (uint8_t i = 0; i < MAX_APPVARS; ++i) {
                char name[10];
                sprintf(name, APPVAR_PREFIX "%u", i);
                if (os_ChkFindSym(OS_TYPE_APPVAR, name, NULL, NULL))
                    delete_var(name, OS_TYPE_APPVAR);
                else break;
            }
            center_text("AppVars + installer removed", 115, PAL_GRAY);
            center_text("Press any key", 135, PAL_GRAY);
        } else {
            center_text("Press any key", 130, PAL_GRAY);
        }
    } else {
        center_text("Install failed", 70, PAL_RED);
        const char *msg = "Unknown error";
        if (error == MISSING_VAR) msg = "Missing atmOS AppVar(s)";
        else if (error == PORT_SETUP_FAILED) msg = "Unsupported OS version";
        else if (error == NO_SPACE) msg = "Not enough archive space";
        else if (error == USER_ABORT) msg = "Cancelled by user";
        center_text(msg, 100, PAL_TEXT);
        center_text("Press any key", 140, PAL_GRAY);
    }
    gfx_SwapDraw();
    wait_key();
    gfx_End();
    os_ClrHome();
}

int main(int argc, char **argv)
{
    (void)argc;
    bool delete_after = true;
    int error = SUCCESS;

    /* fileioc BEFORE graphics: look for existing settings, find font packs */
    bool setup_done = load_prefs();
    if (!setup_done)
        scan_font_packs();

    gfx_Begin();
    apply_theme();
    gfx_SetDrawBuffer();

    /* First-time setup only when no valid SHLSET exists yet; afterwards the
     * user changes everything in the shell's own Settings app. */
    if (!setup_done) {
        if (screen_welcome() != SUCCESS)   { error = USER_ABORT; goto end_gfx; }
        if (screen_customize() != SUCCESS) { error = USER_ABORT; goto end_gfx; }
    }
    if (screen_options(&delete_after) != SUCCESS) { error = USER_ABORT; goto end_gfx; }

end_gfx:
    gfx_End();

    /* fileioc AFTER graphics. Never overwrite settings the user already has. */
    if (error == SUCCESS && !setup_done)
        save_prefs();

    if (error == SUCCESS)
        error = do_install();

    screen_result(error, delete_after && error == SUCCESS);

    if (error == SUCCESS && delete_after) {
        /* Last thing we do: nothing may run after this except the return. */
        delete_var(argv[0], OS_TYPE_PRGM);
    }
    return error;
}