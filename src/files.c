#ifdef SHELL_DISABLE_FILES

#include <graphx.h>
#include <stdbool.h>
#include "files.h"

size_t files_get_pins(files_pin_t *pins, size_t capacity)
{
    (void)pins;
    (void)capacity;
    return 0;
}

bool files_is_pinned(const char *name, uint8_t type)
{
    (void)name;
    (void)type;
    return false;
}

files_pin_result_t files_set_pin(const files_pin_t *pin, bool pinned)
{
    (void)pin;
    (void)pinned;
    return FILES_PIN_STORAGE_ERROR;
}

bool files_take_pins_changed(void)
{
    return false;
}

bool files_load_pin_icon(const files_pin_t *pin, uint8_t pixels[16 * 16])
{
    (void)pin;
    (void)pixels;
    return false;
}

void files_open(void)
{
}

void files_draw(int x, int y)
{
    const char *message = "FILES DISABLED";

    gfx_SetTextScale(1, 1);
    ui_text_color(19);
    gfx_SetTextBGColor(255);
    gfx_SetTextTransparentColor(255);
    ui_print(message, x + (304 - ui_text_width(message)) / 2, y + 80);
}

void files_draw_region(int x, int y, int x0, int y0, int x1, int y1)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    files_draw(x, y);
}

void files_click(int x, int y)
{
    (void)x;
    (void)y;
}

bool files_tick(void)
{
    return false;
}

bool files_hover(int x, int y)
{
    (void)x;
    (void)y;
    return false;
}

bool files_keys(void)
{
    return false;
}

bool files_press(void)
{
    return false;
}

bool files_alpha(void)
{
    return false;
}

bool files_menu_active(void)
{
    return false;
}

bool files_cancel_menu(void)
{
    return false;
}

uint8_t files_program_kind(const char *name, uint8_t type) { (void)name; (void)type; return 2; }
bool files_take_launch_request(const char **prgm, void **app_header,
                               uint8_t *program_kind, uint8_t *var_type)
{
    (void)prgm;
    (void)app_header;
    (void)program_kind;
    (void)var_type;
    return false;
}

#else

/* files.c - Files app for the TI-84 Plus CE shell
 *
 * Pointer-first redesign: instead of a cramped text list, the window is an
 * icon grid with big hit targets, like a home screen inside the window.
 *
 *  - a root page with Programs, AppVars, and Apps folders
 *  - wide tiles for variables; click selects, click again on a program runs it
 *  - hovering past the left/right edges of the grid flips pages;
 *    page dots at the bottom show where you are
 *
 * Hit testing is pointer-driven; arrows/Enter stay free for the shell cursor.
 */

#include <graphx.h>
#ifndef FILES_NO_FILEIOC
#include <fileioc.h>
#include <ti/tokens.h>
#else
#include <ti/vars.h>
#endif
#include <keypadc.h>
#include <tice.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "files.h"
#include "debug_stack.h"
#include "ui_text.h"
#include "program_kind.h"

/* palette indices - must match the enum in main.c */
enum {
    PAL_GRAD0 = 0,
    PAL_BAR = 16, PAL_BAR_LINE = 17, PAL_DOCK = 18,
    PAL_TEXT = 19, PAL_LABEL = 20, PAL_ACCENT = 21,
    PAL_ICON0 = 22                      /* red,yl,grn,blu,pur,cyn,org,gray */
};

/* ---------- layout (content area is WIN_W x 180 at the given origin) ---------- */
#define WIN_W      304
#define CONTENT_H  180

#define TOOL_H     22                   /* folder navigation */
#define PAGE_H     (CONTENT_H - TOOL_H - 8)
#define PAGER_H    8                    /* page dots strip */

#define COLS       3
#define ROWS       2
#define PER_PAGE   (COLS * ROWS)        /* 6 tiles per page */

#define TILE_W     78
#define TILE_H     68
#define GAP        8
#define GRID_X0    (ARROW_W + ((WIN_W - ARROW_W * 2 - (COLS * TILE_W + (COLS - 1) * GAP)) / 2))
#define GRID_Y0    (TOOL_H + (PAGE_H - (ROWS * TILE_H + (ROWS - 1) * GAP)) / 2)

/* page arrows: rounded square buttons beside the grid */
#define ARROW_W    16
#define ARROW_H    22
#define ARROW_Y    (TOOL_H + (PAGE_H - ARROW_H) / 2)

#define NAV_W      86
#define NAV_GAP    4
#define NAV_X0     ((WIN_W - (3 * NAV_W + 2 * NAV_GAP)) / 2)

/* Keep these modest: vars[] is BSS and every byte here is a byte the C stack
 * cannot use. 256+48 was enough to NMI-reset CEmu (stack-limit write) once
 * icon buffers and the main loop shared the same tight stack. */
#define MAX_VARS 64
#define MAX_APPS 16
#define MAX_ITEMS (MAX_VARS + MAX_APPS)
#define OS_TYPE_APP FILES_TYPE_APP
#define ICON_PALETTE_START 30
#define ICON_PALETTE_LEVELS 6
#define ICON_PALETTE_COUNT \
    (ICON_PALETTE_LEVELS * ICON_PALETTE_LEVELS * ICON_PALETTE_LEVELS)
#define ICON_DCS_COLOR_COUNT 24
#define ICON_READ_SIZE 280
#define ICON_CACHE_ENTRIES PER_PAGE

typedef struct {
    char     name[9];
    uint8_t  type;
    uint8_t  program_kind;
    uint24_t size;
    bool     size_valid;
    void    *app_header;
} file_var_t;

typedef enum { F_PROGRAMS, F_APPVARS, F_APPS } folder_t;

static file_var_t vars[MAX_ITEMS];
static int var_count = 0;

/* visible items: indices into vars[], rebuilt when changing folders */
static uint16_t view[MAX_ITEMS];
static int  view_count = 0;
static folder_t cur_folder = F_PROGRAMS;

static int page = 0;             /* current page of tiles */
static int sel = -1;             /* selected index into view[], -1 = none */
static int hover_tile = -1;      /* tile slot 0..PER_PAGE-1 under pointer */
static int  hover_btn = -1;       /* 2/3 = page, 4-6 = folders */
static int  launch_request = -1;  /* selected program awaiting launch by main */
static bool pin_menu_open;
static int pin_menu_choice;
static int pin_menu_item = -1;
static int pin_menu_notice;
static uint16_t pin_menu_prev_keys;
static bool pins_changed;
static bool pin_menu_is_pinned;

#ifndef FILES_NO_ICONS
typedef struct {
    int     var_index;
    bool    has_icon;
    uint8_t pixels[16 * 16];
} icon_cache_entry_t;

static icon_cache_entry_t icon_cache[ICON_CACHE_ENTRIES];
static int icon_cache_page = -1;
static folder_t icon_cache_folder = F_PROGRAMS;
#endif

extern bool flash_apps_find_next(void);
extern void *flash_apps_find_app(const char *name);

#define PIN_APPVAR "SHLPINS"
#define PIN_DATA_VERSION 1
#define PIN_RECORD_SIZE 10
#define PIN_MENU_X ((WIN_W - 196) / 2)
#define PIN_MENU_Y (TOOL_H + 48)
#define PIN_MENU_W 196
#define PIN_MENU_H 58

#if !defined(FILES_NO_FILEIOC) && !defined(FILES_NO_ICONS)
static void setup_program_icon_palette(void);
static bool read_program_icon(uint8_t handle, uint8_t *icon);
static uint8_t ti_icon_color_to_palette(uint8_t color);
#endif

#ifndef FILES_NO_FILEIOC
static size_t read_pins(files_pin_t *pins, size_t capacity)
{
    uint8_t handle, header[6], record[PIN_RECORD_SIZE];
    size_t count, i, copied;

    handle = ti_Open(PIN_APPVAR, "r");
    if (!handle) return 0;
    if (ti_Read(header, 1, sizeof(header), handle) != sizeof(header) ||
        memcmp(header, "SHPN", 4) != 0 ||
        header[4] != PIN_DATA_VERSION ||
        header[5] > FILES_MAX_PINS) {
        ti_Close(handle);
        return 0;
    }

    count = header[5];
    copied = 0;
    for (i = 0; i < count; i++) {
        if (ti_Read(record, 1, sizeof(record), handle) != sizeof(record))
            break;
        if (copied < capacity && pins) {
            files_pin_t *pin = &pins[copied++];
            pin->type = record[0];
            pin->program_kind = record[1];
            memcpy(pin->name, record + 2, 8);
            pin->name[8] = 0;
            if ((pin->type != OS_TYPE_PRGM &&
                 pin->type != OS_TYPE_PROT_PRGM &&
                 pin->type != OS_TYPE_APP) ||
                pin->name[0] < 'A' || pin->name[0] > 'Z') {
                ti_Close(handle);
                return 0;
            }
        }
    }
    ti_Close(handle);
    return i == count ? (pins && capacity < count ? capacity : count) : 0;
}

size_t files_get_pins(files_pin_t *pins, size_t capacity)
{
    return read_pins(pins, capacity);
}

bool files_is_pinned(const char *name, uint8_t type)
{
    files_pin_t pins[FILES_MAX_PINS];
    size_t i, count = files_get_pins(pins, FILES_MAX_PINS);

    for (i = 0; i < count; i++)
        if (pins[i].type == type && strcmp(pins[i].name, name) == 0)
            return true;
    return false;
}

files_pin_result_t files_set_pin(const files_pin_t *pin, bool pinned)
{
    files_pin_t pins[FILES_MAX_PINS];
    uint8_t record[PIN_RECORD_SIZE], header[6] = {
        'S', 'H', 'P', 'N', PIN_DATA_VERSION, 0
    };
    uint8_t handle;
    size_t i, count = files_get_pins(pins, FILES_MAX_PINS);

    for (i = 0; i < count; i++) {
        if (pins[i].type == pin->type && strcmp(pins[i].name, pin->name) == 0)
            break;
    }

    if (pinned) {
        if (i < count) return FILES_PIN_OK;
        if (count == FILES_MAX_PINS) return FILES_PIN_FULL;
        pins[count++] = *pin;
    } else {
        if (i == count) return FILES_PIN_OK;
        for (; i + 1 < count; i++)
            pins[i] = pins[i + 1];
        count--;
    }

    handle = ti_Open(PIN_APPVAR, "w");
    if (!handle) return FILES_PIN_STORAGE_ERROR;
    header[5] = (uint8_t)count;
    if (ti_Write(header, 1, sizeof(header), handle) != sizeof(header))
        goto save_error;
    for (i = 0; i < count; i++) {
        record[0] = pins[i].type;
        record[1] = pins[i].program_kind;
        memcpy(record + 2, pins[i].name, 8);
        if (ti_Write(record, 1, sizeof(record), handle) != sizeof(record))
            goto save_error;
    }
    if (!ti_SetArchiveStatus(true, handle))
        goto save_error;
    if (!ti_Close(handle))
        return FILES_PIN_STORAGE_ERROR;
    pins_changed = true;
    return FILES_PIN_OK;

save_error:
    ti_Close(handle);
    return FILES_PIN_STORAGE_ERROR;
}
#else
size_t files_get_pins(files_pin_t *pins, size_t capacity)
{
    (void)pins;
    (void)capacity;
    return 0;
}

bool files_is_pinned(const char *name, uint8_t type)
{
    (void)name;
    (void)type;
    return false;
}

files_pin_result_t files_set_pin(const files_pin_t *pin, bool pinned)
{
    (void)pin;
    (void)pinned;
    return FILES_PIN_STORAGE_ERROR;
}
#endif

bool files_take_pins_changed(void)
{
    bool changed = pins_changed;
    pins_changed = false;
    return changed;
}

bool files_load_pin_icon(const files_pin_t *pin, uint8_t pixels[16 * 16])
{
#if !defined(FILES_NO_FILEIOC) && !defined(FILES_NO_ICONS)
    setup_program_icon_palette();
    if (pin->type == OS_TYPE_APP) {
        uint8_t *app_code;
        uint8_t *app_icon;
        void *app_header = flash_apps_find_app(pin->name);
        size_t pixel;
        if (!app_header) return false;
        app_code = (uint8_t *)app_header + 0x100;
        app_icon = app_code + *(uint24_t *)(app_code + 0x12);
        if (app_icon[0] != 1) return false;
        for (pixel = 0; pixel < 16 * 16; pixel++) {
            pixels[pixel] = ti_icon_color_to_palette(app_icon[3 + pixel]);
        }
        return true;
    } else {
        uint8_t handle = ti_OpenVar(pin->name, "r", pin->type);
        bool found;
        if (!handle) return false;
        found = read_program_icon(handle, pixels);
        ti_Close(handle);
        return found;
    }
#else
    (void)pin;
    (void)pixels;
    return false;
#endif
}

/* ---------- VAT scanning ---------- */

static const uint8_t program_icon_colors[ICON_DCS_COLOR_COUNT][3] = {
    {   0,   0, 255 }, { 255,   0,   0 }, {   0,   0,   0 },
    { 255,   0, 255 }, {   0, 255,   0 }, { 255, 160,   0 },
    { 128,  64,   0 }, {   0,   0, 128 }, { 128, 192, 255 },
    { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 },
    { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 },
    { 255, 255, 255 }, { 255, 255,   0 }, { 255, 255, 255 },
    { 222, 222, 222 }, { 181, 181, 181 }, { 148, 148, 148 },
    {  74,  74,  74 }, { 255, 255, 255 }, {  86, 160, 132 }
};

static void setup_program_icon_palette(void)
{
    int r, g, b;

    for (r = 0; r < ICON_PALETTE_LEVELS; r++) {
        for (g = 0; g < ICON_PALETTE_LEVELS; g++) {
            for (b = 0; b < ICON_PALETTE_LEVELS; b++) {
                int index = ICON_PALETTE_START +
                    r * ICON_PALETTE_LEVELS * ICON_PALETTE_LEVELS +
                    g * ICON_PALETTE_LEVELS + b;
                gfx_palette[index] = gfx_RGBTo1555(
                    r * 255 / (ICON_PALETTE_LEVELS - 1),
                    g * 255 / (ICON_PALETTE_LEVELS - 1),
                    b * 255 / (ICON_PALETTE_LEVELS - 1));
            }
        }
    }
}

static uint8_t rgb_to_icon_palette(uint8_t r, uint8_t g, uint8_t b)
{
    int ri = (r * (ICON_PALETTE_LEVELS - 1) + 127) / 255;
    int gi = (g * (ICON_PALETTE_LEVELS - 1) + 127) / 255;
    int bi = (b * (ICON_PALETTE_LEVELS - 1) + 127) / 255;

    return (uint8_t)(ICON_PALETTE_START +
        ri * ICON_PALETTE_LEVELS * ICON_PALETTE_LEVELS +
        gi * ICON_PALETTE_LEVELS + bi);
}

/* Maps TI's fixed icon palette indices to the shell's RGB palette. */
static const uint8_t ti_icon_color_map[256] = {
    30, 36, 36, 42, 49, 49, 55, 55, 31, 37, 38, 44, 50, 50, 56, 56,
    33, 39, 39, 45, 51, 51, 58, 58, 34, 40, 40, 46, 53, 53, 59, 59,
    66, 72, 72, 78, 85, 85, 91, 97, 67, 73, 74, 80, 86, 86, 92, 98,
    69, 75, 75, 81, 87, 87, 94, 100, 70, 76, 76, 82, 89, 89, 95, 101,
    66, 72, 72, 78, 85, 85, 91, 97, 67, 73, 74, 80, 86, 86, 92, 98,
    105, 111, 111, 117, 123, 123, 130, 136, 106, 112, 112, 118, 125, 125, 131, 137,
    102, 108, 114, 114, 121, 121, 127, 133, 103, 109, 116, 116, 122, 122, 128, 134,
    105, 111, 117, 117, 123, 123, 130, 136, 106, 112, 118, 118, 125, 125, 131, 137,
    138, 144, 150, 150, 157, 157, 163, 169, 139, 145, 152, 152, 158, 158, 164, 170,
    141, 147, 153, 153, 159, 159, 166, 172, 142, 148, 154, 154, 161, 161, 167, 173,
    138, 144, 150, 150, 157, 163, 163, 169, 139, 145, 152, 152, 158, 164, 164, 170,
    177, 183, 189, 189, 195, 201, 202, 208, 178, 184, 190, 190, 197, 203, 203, 209,
    174, 180, 186, 186, 193, 199, 199, 205, 175, 181, 188, 188, 194, 200, 200, 206,
    177, 183, 189, 189, 195, 201, 202, 208, 178, 184, 190, 190, 197, 203, 203, 209,
    216, 216, 222, 222, 229, 235, 235, 241, 217, 217, 224, 224, 230, 236, 236, 242,
    219, 219, 225, 225, 231, 237, 238, 244, 220, 220, 226, 226, 233, 239, 239, 245
};

static uint8_t ti_icon_color_to_palette(uint8_t color)
{
    return ti_icon_color_map[color];
}

#if !defined(FILES_NO_FILEIOC) && !defined(FILES_NO_ICONS)
static int icon_hex_value(uint8_t ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'H') return 17 + ch - 'A';
    return -1;
}

static int icon_nibble(uint8_t ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
}

static uint8_t color_icon_pixel(uint8_t ch)
{
    int color = icon_hex_value(ch);

    if (color < 0 || color > ICON_DCS_COLOR_COUNT)
        return 255;
    if (color == 0 || (color >= 10 && color <= 16) ||
        color == 18 || color == 23)
        return 255;
    color--;
    return rgb_to_icon_palette(program_icon_colors[color][0],
                               program_icon_colors[color][1],
                               program_icon_colors[color][2]);
}

static bool read_program_icon(uint8_t handle, uint8_t *icon)
{
    /* static: called from the draw path; 280 bytes on the C stack on top of
     * render_frame + files_draw_region is enough to collide with the heap/VAT
     * on a busy Files page and RAM-clear the calc. */
    static uint8_t data[ICON_READ_SIZE];
    size_t size = ti_GetSize(handle);
    NOTE_STACK();
    size_t bytes = size < sizeof(data) ? size : sizeof(data);
    size_t count = ti_Read(data, 1, bytes, handle);
    size_t i, quote, icon_start;

    for (i = 0; i + 3 + 16 * 16 <= count && i < 32; i++) {
        size_t pixel;

        if (data[i] != 0x01 || data[i + 1] != 16 || data[i + 2] != 16)
            continue;

        for (pixel = 0; pixel < 16 * 16; pixel++) {
            uint8_t color = data[i + 3 + pixel];
            icon[pixel] = ti_icon_color_to_palette(color);
        }
        return true;
    }

    if (count < 6) return false;

    /* DCS stores icon pixels as a quoted string following a :DCS line. */
    for (i = 0; i + 5 < count; i++) {
        size_t length;

        if (data[i] != OS_TOK_COLON ||
            data[i + 1] != OS_TOK_D ||
            data[i + 2] != OS_TOK_C ||
            data[i + 3] != OS_TOK_S ||
            data[i + 4] != OS_TOK_NEWLINE ||
            data[i + 5] != OS_TOK_DOUBLE_QUOTE)
            continue;

        icon_start = i + 6;
        for (quote = icon_start; quote < count; quote++) {
            if (data[quote] == OS_TOK_DOUBLE_QUOTE) break;
        }
        if (quote == count) return false;
        length = quote - icon_start;

        memset(icon, 255, 16 * 16);
        if (length == 256) {
            size_t pixel;
            for (pixel = 0; pixel < 16 * 16; pixel++) {
                int color = icon_hex_value(data[icon_start + pixel]);
                if (color < 0 || color > ICON_DCS_COLOR_COUNT)
                    return false;
                icon[pixel] = color_icon_pixel(data[icon_start + pixel]);
            }
            return true;
        }
        if (length == 64) {
            size_t pixel;
            for (pixel = 0; pixel < 64; pixel++) {
                int nibble = icon_nibble(data[icon_start + pixel]);
                int bit;
                if (nibble < 0) return false;
                for (bit = 0; bit < 4; bit++) {
                    size_t x = (pixel % 4) * 4 + bit;
                    size_t y = pixel / 4;
                    if (nibble & (8 >> bit))
                        icon[y * 16 + x] = PAL_TEXT;
                }
            }
            return true;
        }
        if (length == 16) {
            size_t pixel;
            for (pixel = 0; pixel < 16; pixel++) {
                int nibble = icon_nibble(data[icon_start + pixel]);
                int bit;
                if (nibble < 0) return false;
                for (bit = 0; bit < 4; bit++) {
                    size_t x = (pixel % 2) * 8 + bit * 2;
                    size_t y = (pixel / 2) * 2;
                    if (nibble & (8 >> bit)) {
                        icon[y * 16 + x] = PAL_TEXT;
                        icon[y * 16 + x + 1] = PAL_TEXT;
                        icon[(y + 1) * 16 + x] = PAL_TEXT;
                        icon[(y + 1) * 16 + x + 1] = PAL_TEXT;
                    }
                }
            }
            return true;
        }
        return false;
    }

    return false;
}

static void cache_visible_program_icons(void)
{
    int slot;

    if (icon_cache_page == page && icon_cache_folder == cur_folder)
        return;

    for (slot = 0; slot < ICON_CACHE_ENTRIES; slot++) {
        int index = page * PER_PAGE + slot;
        icon_cache_entry_t *cached = &icon_cache[slot];

        cached->var_index = -1;
        cached->has_icon = false;
        if (index >= view_count)
            continue;

        cached->var_index = view[index];
        if (vars[cached->var_index].type == OS_TYPE_PRGM ||
            vars[cached->var_index].type == OS_TYPE_PROT_PRGM) {
            uint8_t handle = ti_OpenVar(vars[cached->var_index].name, "r",
                                        vars[cached->var_index].type);
            if (handle) {
                cached->has_icon = read_program_icon(handle, cached->pixels);
                ti_Close(handle);
            }
        }
    }

    icon_cache_page = page;
    icon_cache_folder = cur_folder;
}
#endif

static void add_var(const char *name, uint8_t type)
{
#ifndef FILES_NO_FILEIOC
    uint8_t handle;
#else
    size_t size;
#endif
    file_var_t *v;

    if (var_count >= MAX_ITEMS || !name) return;
    v = &vars[var_count];
    strncpy(v->name, name, 8);
    v->name[8] = 0;
    v->type = type;
    v->program_kind = PROGRAM_BASIC;
    v->size = 0;
    v->size_valid = false;
    v->app_header = NULL;

#ifndef FILES_NO_FILEIOC
    if (type != OS_TYPE_APP) {
        handle = ti_OpenVar(name, "r", type);
        if (handle) {
            v->size = (uint24_t)ti_GetSize(handle);
            v->size_valid = true;
            if (type == OS_TYPE_PRGM || type == OS_TYPE_PROT_PRGM) {
                uint8_t signature[5];
                size_t count = ti_Read(signature, 1, sizeof(signature), handle);

                v->program_kind = program_kind_from_signature(signature, count);
            }
            ti_Close(handle);
        }
    }
#else
    if (type != OS_TYPE_APP && os_GetVarSize(name, &size) == 0) {
        v->size = (uint24_t)size;
        v->size_valid = true;
    }
#endif
    var_count++;
}

static void scan_flash_apps(void)
{
    /* FindAppUp uses OP1 as its iterator state. */
    volatile uint8_t *op1 = (volatile uint8_t *)0xD005F8;
    int app_count = 0;

    memset((void *)op1, 0, 9);
    op1[0] = OS_TYPE_APP;

    while (var_count < MAX_ITEMS && app_count < MAX_APPS &&
           flash_apps_find_next()) {
        uint8_t saved_op1[9];
        char name[9];
        void *header;
        int i;

        memcpy(saved_op1, (const void *)op1, sizeof(saved_op1));
        memcpy(name, (const void *)(op1 + 1), 8);
        name[8] = 0;
        for (i = 7; i >= 0 && name[i] == ' '; i--)
            name[i] = 0;
        if (!name[0] || name[0] < 'A' || strcmp(name, SHELL_APP_NAME) == 0)
            continue;

        header = flash_apps_find_app(name);
        /* Keep the OS app-name search state intact across this lookup. */
        memcpy((void *)op1, saved_op1, sizeof(saved_op1));
        if (!header)
            continue;

        add_var(name, OS_TYPE_APP);
        vars[var_count - 1].app_header = header;
        app_count++;
    }
}

/* ti_DetectVar only scans RAM; ti_DetectArchiveVar only scans the archive.
 * ti_DetectAny walks the ENTIRE VAT (RAM + archive) in one pass, reporting
 * each entry's type and archived flag - works on old toolchains too. */
static void rescan(void)
{
#ifndef FILES_NO_FILEIOC
    void    *vat = NULL;
    uint8_t  type;
    char    *name;
#else
    void    *entry;
    void    *data;
    uint24_t type;
    uint24_t name_length;
    char     name[9];
#endif
    int      i;

    var_count = 0;

#ifndef FILES_NO_FILEIOC
    while (var_count < MAX_VARS &&
           (name = ti_DetectAny(&vat, NULL, &type)) != NULL) {
        if ((type == OS_TYPE_PRGM || type == OS_TYPE_PROT_PRGM ||
             type == OS_TYPE_APPVAR) && name[0] >= 'A' &&
            strcmp(name, SHELL_APP_NAME) != 0) {
            add_var(name, type);
        }
    }
#else
    entry = os_GetSymTablePtr();
    while (var_count < MAX_VARS &&
           (entry = os_NextSymEntry(entry, &type, &name_length, name, &data)) != NULL) {
        if (name_length == 0 || name_length > 8)
            continue;
        name[name_length] = 0;
        if ((type == OS_TYPE_PRGM || type == OS_TYPE_PROT_PRGM ||
             type == OS_TYPE_APPVAR) && name[0] >= 'A' &&
            strcmp(name, SHELL_APP_NAME) != 0) {
            add_var(name, (uint8_t)type);
        }
    }
#endif
    scan_flash_apps();

    /* Alphabetical order */
    for (i = 1; i < var_count; i++) {
        file_var_t v = vars[i];
        int j = i - 1;

        while (j >= 0 && strcmp(vars[j].name, v.name) > 0) {
            vars[j + 1] = vars[j];
            j--;
        }

        vars[j + 1] = v;
    }
}

static void rebuild_view(void)
{
    int i;
    view_count = 0;
    for (i = 0; i < var_count; i++) {
        bool matches = cur_folder == F_PROGRAMS
            ? vars[i].type == OS_TYPE_PRGM || vars[i].type == OS_TYPE_PROT_PRGM
            : cur_folder == F_APPVARS
                ? vars[i].type == OS_TYPE_APPVAR
                : vars[i].type == OS_TYPE_APP;
        if (matches)
            view[view_count++] = i;
    }

    if (sel >= view_count) sel = -1;
    if (view_count) {
        int pages = (view_count + PER_PAGE - 1) / PER_PAGE;
        if (page >= pages) page = pages - 1;
    } else {
        page = 0;
        sel = -1;
    }
    launch_request = -1;
}

/* ---------- helpers ---------- */

static uint8_t type_color(uint8_t type)
{
    switch (type) {
    case OS_TYPE_APP:       return PAL_ICON0 + 3;
    case OS_TYPE_PRGM:      return PAL_ICON0 + 3;   /* blue */
    case OS_TYPE_PROT_PRGM: return PAL_ICON0 + 6;   /* orange */
    default:          return PAL_ICON0 + 4;   /* purple */
    }
}

static void fill_round(int x, int y, int w, int h, int r);

static void draw_file_icon(uint8_t type, int x, int y)
{
    int bx = x + (TILE_W - 34) / 2;
    int ix = x + (TILE_W - 18) / 2, iy = y + 3;

    gfx_SetColor(type_color(type));
    fill_round(bx, y + 2, 34, 24, 5);
    gfx_SetColor(PAL_LABEL);
    fill_round(ix, iy, 18, 21, 2);

    if (type == OS_TYPE_APP) {
        gfx_SetColor(PAL_TEXT);
        gfx_FillRectangle(ix + 5, iy + 5, 9, 11);
        gfx_SetColor(PAL_LABEL);
        gfx_FillRectangle(ix + 7, iy + 7, 5, 2);
        gfx_FillRectangle(ix + 7, iy + 11, 5, 2);
    } else if (type == OS_TYPE_PRGM) {
        gfx_SetColor(PAL_TEXT);
        gfx_FillTriangle(ix + 5, iy + 5, ix + 5, iy + 16, ix + 14, iy + 10);
    } else if (type == OS_TYPE_PROT_PRGM) {
        gfx_SetColor(PAL_TEXT);
        gfx_FillRectangle(ix + 5, iy + 11, 9, 7);
        gfx_FillRectangle(ix + 7, iy + 7, 2, 5);
        gfx_FillRectangle(ix + 11, iy + 7, 2, 5);
        gfx_FillRectangle(ix + 9, iy + 13, 2, 3);
    } else {
        gfx_SetColor(type_color(type));
        gfx_FillRectangle(ix + 4, iy + 5, 4, 4);
        gfx_FillRectangle(ix + 10, iy + 5, 4, 4);
        gfx_FillRectangle(ix + 4, iy + 11, 4, 4);
        gfx_FillRectangle(ix + 10, iy + 11, 4, 4);
    }
}

#ifndef FILES_NO_ICONS
/* The icon is drawn at 2x. A full 32x32 sprite would be a 1 KB local on a stack that only has
 * a few KB in total, so it is built and drawn in four 32x8 bands (258 bytes at a time).
 * band_data is static for the same reason as read_program_icon's buffer: this runs
 * from the draw path and must not push SP into the heap/VAT. */
static void draw_program_icon(const uint8_t *icon, int x, int y)
{
    static uint8_t band_data[2 + 32 * 8];
    int dx = x + (TILE_W - 32) / 2, dy = y + 2;
    int band, i, j;

    NOTE_STACK();
    band_data[0] = 32;
    band_data[1] = 8;
    for (band = 0; band < 4; band++) {
        for (i = 0; i < 4; i++) {
            const uint8_t *src = icon + (band * 4 + i) * 16;
            uint8_t *row = band_data + 2 + i * 2 * 32;
            for (j = 0; j < 16; j++) {
                uint8_t pixel = src[j];
                row[j * 2] = pixel;
                row[j * 2 + 1] = pixel;
                row[32 + j * 2] = pixel;
                row[32 + j * 2 + 1] = pixel;
            }
        }
        gfx_TransparentSprite((gfx_sprite_t *)band_data, dx, dy + band * 8);
    }
}
#endif

static void tile_pos(int slot, int *x, int *y)
{
    *x = GRID_X0 + (slot % COLS) * (TILE_W + GAP);
    *y = GRID_Y0 + (slot / COLS) * (TILE_H + GAP);
}

/* which tile slot is at (x,y)? -1 if none */
static int tile_at(int x, int y)
{
    int c, r;
    if (y < GRID_Y0 || y >= GRID_Y0 + ROWS * TILE_H + (ROWS - 1) * GAP) return -1;
    if (x < GRID_X0 || x >= GRID_X0 + COLS * TILE_W + (COLS - 1) * GAP) return -1;
    c = (x - GRID_X0) / (TILE_W + GAP);
    r = (y - GRID_Y0) / (TILE_H + GAP);
    if (x - GRID_X0 - c * (TILE_W + GAP) >= TILE_W) return -1;   /* in a gap */
    if (y - GRID_Y0 - r * (TILE_H + GAP) >= TILE_H) return -1;
    return r * COLS + c;
}

static int nav_at(int x, int y)
{
    int i;
    if (y < 2 || y >= TOOL_H) return -1;
    for (i = 0; i < 3; i++) {
        int nx = NAV_X0 + i * (NAV_W + NAV_GAP);
        if (x >= nx && x < nx + NAV_W) return i;
    }
    return -1;
}

/* page arrow rects: btn 2 = left, 3 = right */
static void arrow_rect(int btn, int *ax, int *ay)
{
    *ax = btn == 2 ? 1 : WIN_W - ARROW_W - 1;
    *ay = ARROW_Y;
}

static int page_count(void)
{
    return (view_count + PER_PAGE - 1) / PER_PAGE;
}

static bool pin_menu_pinnable(const file_var_t *v)
{
    return v->type == OS_TYPE_PRGM || v->type == OS_TYPE_PROT_PRGM ||
           v->type == OS_TYPE_APP;
}

static void pin_menu_activate(void)
{
    file_var_t *v;
    files_pin_t pin;
    files_pin_result_t result;
    bool should_pin;

    if (!pin_menu_open || pin_menu_item < 0 ||
        pin_menu_item >= view_count)
        return;
    if (pin_menu_choice != 0) {
        pin_menu_open = false;
        pin_menu_notice = 0;
        return;
    }

    v = &vars[view[pin_menu_item]];
    should_pin = !pin_menu_is_pinned;
    strncpy(pin.name, v->name, sizeof(pin.name));
    pin.name[sizeof(pin.name) - 1] = 0;
    pin.type = v->type;
    pin.program_kind = v->program_kind;
    result = files_set_pin(&pin, should_pin);
    if (result == FILES_PIN_OK) {
        pin_menu_open = false;
        pin_menu_notice = 0;
    } else {
        pin_menu_notice = result == FILES_PIN_FULL ? 1 : 2;
    }
}

/* ---------- app_t interface ---------- */

void files_open(void)
{
    rescan();
    page = 0;
    sel = -1;
    hover_tile = hover_btn = -1;
    cur_folder = F_PROGRAMS;
    launch_request = -1;
    pin_menu_open = false;
    pin_menu_notice = 0;
    pin_menu_prev_keys = 0;
#ifndef FILES_NO_ICONS
    icon_cache_page = -1;
#endif
    setup_program_icon_palette();
    rebuild_view();
}

bool files_hover(int x, int y)
{
    int t = -1, b = -1, i, nav;

    if (pin_menu_open) {
        int choice = pin_menu_choice;
        if (x >= PIN_MENU_X && x < PIN_MENU_X + PIN_MENU_W &&
            y >= PIN_MENU_Y + 5 && y < PIN_MENU_Y + 43)
            pin_menu_choice = y < PIN_MENU_Y + 24 ? 0 : 1;
        return choice != pin_menu_choice;
    }

    nav = nav_at(x, y);
    if (nav >= 0) {
        hover_btn = 4 + nav;
        hover_tile = -1;
        return false;
    }

    /* page arrows */
    for (i = 2; i <= 3; i++) {
        int ax, ay;
        arrow_rect(i, &ax, &ay);
        if (x >= ax && x < ax + ARROW_W && y >= ay && y < ay + ARROW_H) b = i;
    }
    if (b == 2 && page == 0)              b = -1;   /* disabled at the ends */
    if (b == 3 && page >= page_count() - 1) b = -1;

    t = tile_at(x, y);
    if (t >= 0 && page * PER_PAGE + t >= view_count) {
        t = -1;
    }

    hover_tile = t;
    hover_btn = b;

    /* Avoid redrawing the whole window just to animate hover feedback. */
    return false;
}

void files_click(int x, int y)
{
    if (pin_menu_open) {
        if (x >= PIN_MENU_X && x < PIN_MENU_X + PIN_MENU_W &&
            y >= PIN_MENU_Y + 8 && y < PIN_MENU_Y + 44) {
            pin_menu_choice = y < PIN_MENU_Y + 27 ? 0 : 1;
            pin_menu_activate();
        } else {
            pin_menu_open = false;
            pin_menu_notice = 0;
        }
        return;
    }

    if (hover_btn >= 4 && hover_btn <= 6) {
        cur_folder = (folder_t)(hover_btn - 4);
        page = 0;
        sel = -1;
        hover_tile = hover_btn = -1;
        rebuild_view();
        return;
    }

    /* page arrows */
    if (hover_btn == 2 && page > 0) {
        page--;
        hover_tile = -1;
        return;
    }
    if (hover_btn == 3 && page < page_count() - 1) {
        page++;
        hover_tile = -1;
        return;
    }

    /* files */
    if (hover_tile >= 0) {
        int idx = page * PER_PAGE + hover_tile;
        if (idx >= view_count) return;

        if (idx == sel) {
            if (vars[view[idx]].type != OS_TYPE_APPVAR) {
                launch_request = idx;
            }
        } else {
            sel = idx;               /* first click: select */
            launch_request = -1;
        }
    }
}

bool files_tick(void)
{
    return false;   /* everything is pointer-driven */
}

bool files_keys(void)
{
    uint16_t keys = kb_Data[7];
    bool up = (keys & kb_Up) && !(pin_menu_prev_keys & kb_Up);
    bool down = (keys & kb_Down) && !(pin_menu_prev_keys & kb_Down);
    pin_menu_prev_keys = keys;
    if (!pin_menu_open) return false;
    if (up) pin_menu_choice = 0;
    if (down) pin_menu_choice = 1;
    return true;
}

bool files_press(void)
{
    if (!pin_menu_open) return false;
    pin_menu_activate();
    return true;
}

bool files_alpha(void)
{
    int idx = -1;
    if (pin_menu_open) return true;
    if (hover_tile >= 0)
        idx = page * PER_PAGE + hover_tile;
    else if (sel >= 0)
        idx = sel;
    if (idx < 0 || idx >= view_count ||
        !pin_menu_pinnable(&vars[view[idx]]))
        return false;

    sel = idx;
    pin_menu_item = idx;
    pin_menu_choice = 0;
    pin_menu_notice = 0;
    pin_menu_prev_keys = kb_Data[7];
    pin_menu_is_pinned = files_is_pinned(vars[view[idx]].name,
                                         vars[view[idx]].type);
    pin_menu_open = true;
    return true;
}

bool files_menu_active(void)
{
    return pin_menu_open;
}

bool files_cancel_menu(void)
{
    if (!pin_menu_open) return false;
    pin_menu_open = false;
    pin_menu_notice = 0;
    return true;
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

static void num_str(uint24_t v, char *out)
{
    char tmp[8];
    int n = 0, i = 0;
    if (!v) { out[0] = '0'; out[1] = 0; return; }
    while (v) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) out[i++] = tmp[--n];
    out[i] = 0;
}

static bool overlaps(int x0, int y0, int x1, int y1,
                     int bx0, int by0, int bx1, int by1)
{
    return x0 < bx1 && bx0 < x1 && y0 < by1 && by0 < y1;
}

static void draw_toolbar(int x, int y)
{
    static const char *const labels[3] = { "PRGMS", "APPVARS", "APPS" };
    static const uint8_t colors[3] = {
        PAL_ICON0 + 3, PAL_ICON0 + 4, PAL_ICON0 + 6
    };
    int i, nx;

    gfx_SetTextScale(1, 1);
    gfx_SetTextBGColor(PAL_BAR);
    gfx_SetTextTransparentColor(PAL_BAR);
    for (i = 0; i < 3; i++) {
        nx = x + NAV_X0 + i * (NAV_W + NAV_GAP);
        gfx_SetColor((int)cur_folder == i ? PAL_ACCENT : PAL_BAR_LINE);
        fill_round(nx, y + 2, NAV_W, 18, 4);

        gfx_SetColor(colors[i]);
        gfx_FillRectangle(nx + 7, y + 7, 12, 3);
        fill_round(nx + 5, y + 9, 16, 7, 2);

        ui_text_color((int)cur_folder == i ? PAL_LABEL : PAL_TEXT);
        ui_print(labels[i], nx + 26, y + 7);
    }
}

static void draw_tile(const file_var_t *v, const uint8_t *icon,
                      int x, int y, bool selected)
{
    char size_s[8];
#ifdef FILES_NO_ICONS
    (void)icon;
#endif

    /* Selection outline */
    if (selected) {
        gfx_SetColor(PAL_ACCENT);
        fill_round(x - 2, y - 2, TILE_W + 4, TILE_H + 4, 8);
    }

    gfx_SetColor(PAL_BAR);
    fill_round(x, y, TILE_W, TILE_H, 7);
#ifndef FILES_NO_ICONS
    if (icon)
        draw_program_icon(icon, x, y);
    else
#endif
        draw_file_icon(v->type, x, y);

    gfx_SetTextScale(1, 1);
    gfx_SetTextBGColor(PAL_BAR);
    gfx_SetTextTransparentColor(PAL_BAR);
    ui_text_color(PAL_TEXT);
    ui_print(v->name,
        x + (TILE_W - ui_text_width(v->name)) / 2,
        y + 36);

    if (v->type == OS_TYPE_APP)
        strcpy(size_s, "APP");
    else if (v->size_valid)
        num_str(v->size, size_s);
    else
        strcpy(size_s, "?");
    ui_print(size_s,
        x + (TILE_W - ui_text_width(size_s)) / 2,
        y + 36 + (int)ui_text_height() + 3);
}

static void draw_grid_region(int x, int y,
                             int x0, int y0, int x1, int y1)
{
    setup_program_icon_palette();
#ifndef FILES_NO_ICONS
    cache_visible_program_icons();
#endif
    int slot, idx;

    for (slot = 0; slot < PER_PAGE; slot++) {
        idx = page * PER_PAGE + slot;
        if (idx >= view_count) break;
        int tx, ty;
        tile_pos(slot, &tx, &ty);
        if (overlaps(x0, y0, x1, y1,
                     x + tx - 2, y + ty - 2,
                     x + tx + TILE_W + 2, y + ty + TILE_H + 2)) {
#ifndef FILES_NO_ICONS
            icon_cache_entry_t *cached = &icon_cache[slot];
            const uint8_t *icon = cached->var_index == view[idx] &&
                                  cached->has_icon ? cached->pixels : NULL;
            draw_tile(&vars[view[idx]], icon, x + tx, y + ty, idx == sel);
#else
            draw_tile(&vars[view[idx]], NULL, x + tx, y + ty, idx == sel);
#endif
        }
    }

    if (!view_count) {
        const char *line1 = cur_folder == F_APPS
            ? "No Flash Apps found"
            : "No files found";
        const char *line2 = "";
        int w1 = ui_text_width(line1);
        int w2 = ui_text_width(line2);
        int tx = x + (WIN_W - w1) / 2;
        int ty = y + CONTENT_H / 2 - (line2[0] ? 8 : 4);
        if (!overlaps(x0, y0, x1, y1, x, ty, x + WIN_W, ty + 18))
            return;
        gfx_SetTextBGColor(PAL_BAR);
        gfx_SetTextTransparentColor(PAL_BAR);
        ui_text_color(PAL_TEXT);
        ui_print(line1, tx, ty);
        if (line2[0])
            ui_print(line2, x + (WIN_W - w2) / 2, ty + 10);
    }
}

static void draw_pager(int x, int y)
{
    int pages = (view_count + PER_PAGE - 1) / PER_PAGE;
    int i, dx;

    if (pages <= 1) return;
    dx = x + WIN_W / 2 - pages * 3;

    for (i = 0; i < pages; i++) {
        gfx_SetColor(i == page ? PAL_ACCENT : PAL_BAR_LINE);
        gfx_FillCircle(dx + i * 6, y + CONTENT_H - 6, 1);
    }
}

static void draw_arrow(int x, int y, int btn)
{
    int ax, ay, cx, cy;
    int tri = btn == 2 ? -1 : 1;    /* left points left, right points right */

    arrow_rect(btn, &ax, &ay);

    /* rounded square page button */
    gfx_SetColor(PAL_DOCK);
    fill_round(x + ax, y + ay, ARROW_W, ARROW_H, 5);

    /* triangle arrow head */
    cx = x + ax + ARROW_W / 2 - tri * 2;
    cy = y + ay + ARROW_H / 2;
    gfx_SetColor(PAL_TEXT);
    gfx_FillTriangle(cx, cy, cx + tri * 5, cy - 4, cx + tri * 5, cy + 4);
}

static void draw_pin_menu(int x, int y)
{
    const char *action = pin_menu_is_pinned
        ? "Unpin from taskbar" : "Pin to taskbar";
    const char *notice = pin_menu_notice == 1 ? "Taskbar is full"
        : pin_menu_notice == 2 ? "Could not save pin" : NULL;
    int menu_x = x + PIN_MENU_X, menu_y = y + PIN_MENU_Y;

    gfx_SetColor(PAL_TEXT);
    fill_round(menu_x, menu_y, PIN_MENU_W, PIN_MENU_H, 5);
    gfx_SetColor(PAL_BAR);
    gfx_FillRectangle(menu_x + 2, menu_y + 2,
                      PIN_MENU_W - 4, PIN_MENU_H - 4);
    gfx_SetTextBGColor(PAL_BAR);
    gfx_SetTextTransparentColor(PAL_BAR);
    gfx_SetTextScale(1, 1);

    gfx_SetColor(pin_menu_choice == 0 ? PAL_ACCENT : PAL_BAR);
    gfx_FillRectangle(menu_x + 4, menu_y + 5, PIN_MENU_W - 8, 18);
    ui_text_color(pin_menu_choice == 0 ? PAL_LABEL : PAL_TEXT);
    ui_print(action, menu_x + 10, menu_y + 10);

    gfx_SetColor(pin_menu_choice == 1 ? PAL_ACCENT : PAL_BAR);
    gfx_FillRectangle(menu_x + 4, menu_y + 25, PIN_MENU_W - 8, 18);
    ui_text_color(pin_menu_choice == 1 ? PAL_LABEL : PAL_TEXT);
    ui_print("Cancel", menu_x + 10, menu_y + 30);

    if (notice) {
        ui_text_color(PAL_ICON0);
        ui_print(notice,
            menu_x + (PIN_MENU_W - ui_text_width(notice)) / 2,
            menu_y + 46);
    }
}

void files_draw_region(int x, int y, int x0, int y0, int x1, int y1)
{
    if (!overlaps(x0, y0, x1, y1, x, y, x + WIN_W, y + CONTENT_H))
        return;

    gfx_SetTextScale(1, 1);
    gfx_SetTextConfig(gfx_text_clip);
    ui_spill_reset();

    if (overlaps(x0, y0, x1, y1,
                 x, y, x + WIN_W, y + TOOL_H))
        draw_toolbar(x, y);
    draw_grid_region(x, y, x0, y0, x1, y1);
    if (overlaps(x0, y0, x1, y1,
                 x, y + CONTENT_H - 10, x + WIN_W, y + CONTENT_H))
        draw_pager(x, y);

    /* page arrows (only when there is more than one page) */
    if (page_count() > 1) {
        int ax, ay;
        if (page > 0) {
            arrow_rect(2, &ax, &ay);
            if (overlaps(x0, y0, x1, y1,
                         x + ax, y + ay, x + ax + ARROW_W, y + ay + ARROW_H))
                draw_arrow(x, y, 2);
        }
        if (page < page_count() - 1) {
            arrow_rect(3, &ax, &ay);
            if (overlaps(x0, y0, x1, y1,
                         x + ax, y + ay, x + ax + ARROW_W, y + ay + ARROW_H))
                draw_arrow(x, y, 3);
        }
    }

    if (pin_menu_open) {
        int mx0 = x + PIN_MENU_X, my0 = y + PIN_MENU_Y;
        int mx1 = mx0 + PIN_MENU_W, my1 = my0 + PIN_MENU_H;
        bool in_region = overlaps(x0, y0, x1, y1, mx0, my0, mx1, my1);
        /* file names under the menu are drawn whole and can land on it outside the repaint
         * rectangle: then the menu is repainted completely (clip widened to the menu) */
        bool spilled = ui_spill_hits(mx0, my0, mx1, my1);

        if (in_region || spilled) {
            if (spilled) ui_overlay_clip(mx0, my0, mx1, my1);
            draw_pin_menu(x, y);
            ui_overlay_clip_end();
        }
    }
}

void files_draw(int x, int y)
{
    files_draw_region(x, y, 0, 0, GFX_LCD_WIDTH, GFX_LCD_HEIGHT);
}

/* Looks at the program now (the kind stored in a pin may be old, or the program may have been
 * replaced since) and says how to run it. */
uint8_t files_program_kind(const char *name, uint8_t type)
{
    uint8_t signature[5];
    uint8_t handle = ti_OpenVar(name, "r", type);
    size_t count;

    if (!handle) return PROGRAM_BASIC;
    count = ti_Read(signature, 1, sizeof(signature), handle);
    ti_Close(handle);
    return program_kind_from_signature(signature, count);
}

/* Returns true after a selected file is clicked again. */
bool files_take_launch_request(const char **prgm_name, void **app_header,
                               uint8_t *program_kind, uint8_t *var_type)
{
    if (launch_request < 0) return false;
    file_var_t *v = &vars[view[launch_request]];
    *prgm_name = v->name;
    *app_header = v->app_header;
    *program_kind = v->program_kind;
    *var_type = v->type;
    launch_request = -1;
    return true;
}

#endif