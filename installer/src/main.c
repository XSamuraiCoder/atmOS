/*
 * atmOS Installer – live themes (switch) + SHLSET via fileioc
 * fileioc only used outside GraphX
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <graphx.h>
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
#define STORE_NAME    "SHLSET"
#define STORE_VERSION 3

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
static uint8_t pref_theme = 0;   /* 0..5 */

/* ---------- SHLSET (call only when GraphX is off) ---------- */
static void load_prefs(void)
{
    uint8_t buf[12];
    uint8_t h = ti_Open(STORE_NAME, "r");
    if (!h) return;
    if (ti_Read(buf, 1, 11, h) >= 11 && buf[1] < 6)
        pref_theme = buf[1];
    ti_Close(h);
}

static void save_prefs(void)
{
    uint8_t buf[21] = {0};
    buf[0] = STORE_VERSION;
    buf[1] = pref_theme;
    /* rest zero = defaults for wall/font/etc. */

    uint8_t h = ti_Open(STORE_NAME, "w");
    if (!h) return;
    ti_Write(buf, 1, 21, h);
    ti_SetArchiveStatus(true, h);
    ti_Close(h);
}

/* ---------- palette (switch, no tables) ---------- */
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

static void apply_theme(void)
{
    uint8_t t0, t1, t2, b0, b1, b2, a0, a1, a2;
    int i;

    switch (pref_theme) {
        case 1:  t0=10;  t1=60;  t2=120; b0=70;  b1=190; b2=210; a0=0;   a1=150; a2=199; break;
        case 2:  t0=24;  t1=70;  t2=52;  b0=150; b1=190; b2=110; a0=40;  a1=160; a2=80;  break;
        case 3:  t0=60;  t1=30;  t2=110; b0=220; b1=110; b2=170; a0=150; a1=70;  a2=220; break;
        case 4:  t0=40;  t1=40;  t2=46;  b0=150; b1=150; b2=160; a0=100; a1=110; a2=130; break;
        case 5:  t0=250; t1=120; t2=160; b0=255; b1=210; b2=120; a0=240; a1=70;  a2=120; break;
        default: t0=60;  t1=100; t2=190; b0=200; b1=120; b2=170; a0=0;   a1=122; a2=255; break;
    }

    for (i = 0; i < 16; i++) {
        set_pal(PAL_GRAD0 + i,
            t0 + ((int)b0 - (int)t0) * i / 15,
            t1 + ((int)b1 - (int)t1) * i / 15,
            t2 + ((int)b2 - (int)t2) * i / 15);
    }
    set_pal(PAL_BAR,      236, 236, 240);
    set_pal(PAL_BAR_LINE, 190, 190, 198);
    set_pal(PAL_DOCK,     225, 225, 235);
    set_pal(PAL_TEXT,      30,  30,  34);
    set_pal(PAL_LABEL,    255, 255, 255);
    set_pal(PAL_ACCENT,   a0, a1, a2);
    set_pal(PAL_RED,      255,  95,  86);
    set_pal(PAL_GREEN,     39, 201,  63);
    set_pal(PAL_GRAY,     142, 142, 147);
}

static const char *theme_label(void)
{
    switch (pref_theme) {
        case 1:  return "Ocean";
        case 2:  return "Forest";
        case 3:  return "Grape";
        case 4:  return "Mono";
        case 5:  return "Candy";
        default: return "Sunset";
    }
}

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
            if (appvar->size != APPVAR_SPLIT_SIZE) break;
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
    center_text("Please wait – do not reset", 130, PAL_GRAY);
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

static bool wait_key(void)
{
    while (kb_AnyKey()) kb_Scan();
    while (!kb_AnyKey()) {
        kb_Scan();
        if (kb_Data[6] & kb_Clear) return false;
    }
    return true;
}

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
            center_text("Interactive setup", 70, PAL_GRAY);
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
        if (kb_Data[6] & kb_Enter)
            return (sel == 1) ? SUCCESS : USER_ABORT;
    }
}

static int screen_customize(void)
{
    int sel = 0;
    int prev_sel = -1;
    uint8_t shown_theme = 255;

    while (kb_AnyKey()) kb_Scan();

    while (1) {
        if (pref_theme != shown_theme) {
            apply_theme();
            draw_wallpaper();
            shown_theme = pref_theme;
            prev_sel = -1;
        }

        if (sel != prev_sel) {
            draw_panel(12, 20, 296, 200);
            center_text("Choose Theme", 32, PAL_TEXT);
            draw_button(40, 70, 240, 28, theme_label(), sel == 0);
            draw_button(70, 130, 180, 28, "Continue", sel == 1);
            center_text("Up/Down move  Left/Right change", 195, PAL_GRAY);
            gfx_SwapDraw();
            prev_sel = sel;
        }

        kb_Scan();

        if (kb_Data[7] & kb_Up) {
            if (sel > 0) sel--;
            while (kb_AnyKey()) kb_Scan();
        }
        if (kb_Data[7] & kb_Down) {
            if (sel < 1) sel++;
            while (kb_AnyKey()) kb_Scan();
        }
        if (kb_Data[6] & kb_Clear)
            return USER_ABORT;

        if (sel == 0 && ((kb_Data[7] & kb_Left) || (kb_Data[7] & kb_Right))) {
            if (kb_Data[7] & kb_Right)
                pref_theme = (pref_theme + 1) % 6;
            else
                pref_theme = (pref_theme + 5) % 6;
            while (kb_AnyKey()) kb_Scan();
        }

        if ((kb_Data[6] & kb_Enter) && sel == 1)
            return SUCCESS;
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
            draw_button(40, 136, 240, 26, "Delete AppVars only", sel == 1);
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
            return SUCCESS;
        }
    }
}

static void screen_result(int error, bool delete_after, const char *prgm_name)
{
    (void)prgm_name;
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
            center_text("AppVars deleted", 115, PAL_GRAY);
            center_text("Delete installer from MEM", 130, PAL_GRAY);
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
}

int main(int argc, char **argv)
{
    (void)argc;
    bool delete_after = true;
    int error = SUCCESS;

    /* fileioc BEFORE graphics */
    load_prefs();

    gfx_Begin();
    apply_theme();
    gfx_SetDrawBuffer();

    if (screen_welcome() != SUCCESS) { error = USER_ABORT; goto end_gfx; }
    if (screen_customize() != SUCCESS) { error = USER_ABORT; goto end_gfx; }
    if (screen_options(&delete_after) != SUCCESS) { error = USER_ABORT; goto end_gfx; }

end_gfx:
    gfx_End();

    /* fileioc AFTER graphics */
    if (error == SUCCESS)
        save_prefs();

    if (error == SUCCESS)
        error = do_install();

    screen_result(error, delete_after && error == SUCCESS, argv[0]);
    return error;
}