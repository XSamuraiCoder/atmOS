/* mods.c - loads atmOS mods (machine-code AppVars) and gives them the API table.
 *
 * Diagnostic build: compile with -DMODS_SKIP_ENTRY (add it to CFLAGS in the Makefile)
 * to load + relocate every mod but never call mod_main. If the shell boots fine that
 * way with mods installed, the crash is inside the relocated mod code (i.e. the
 * mod's relocation table - see the diagnostic mkmod.py on the mod side); if it
 * still crashes, the problem is in the loading/relocation code below.
 *
 * Pointer validation: mods are linked at address 0, so a pointer the mod build
 * forgot to relocate still holds its small link-time value (e.g. 0x0C2A). A call
 * or a write through it lands in flash - CEmu reports "NMI reset caused by writing
 * to flash ... from unprivileged code". api_register_app therefore rejects an app
 * whose pointers do not lie inside a loaded mod's memory: the app is dropped, not
 * the calculator. */
#include <fileioc.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "debug_stack.h"
#include "mods.h"
#include "settings.h"
#include "ui_text.h"

#ifndef MODS_CALL_ENTRY
#define MODS_CALL_ENTRY(entry, api) ((atmos_mod_main_t)(entry))(api)
#endif

static atmos_app_t registered[MODS_MAX_APPS];
static uint8_t     registered_count;
static bool        redraw_requested;
static bool        close_requested;

/* Runtime version of the MODS_SKIP_ENTRY diagnostic: main.c clears this when the
 * shell has no stack of its own (OS stack in use), so mods are still loaded and
 * relocated - for the crash-guard bookkeeping - but mod_main is never called. */
static bool entry_allowed = true;

void mods_set_entry_allowed(bool allowed)
{
    entry_allowed = allowed;
}

/* Memory ranges of loaded mods, filled by load_mod just before mod_main runs.
 * Every pointer a mod hands to the shell must point inside one of these; anything
 * else is an unrelocated pointer (it still holds its link-time offset from 0). */
static struct { const char *base, *end; } mod_range[MODS_MAX_FOUND];
static uint8_t mod_range_n;
static uint8_t rejected_count;

static bool ptr_in_mods(const void *p)
{
    uint8_t i;
    for (i = 0; i < mod_range_n; i++)
        if (p >= (const void *)mod_range[i].base && p < (const void *)mod_range[i].end)
            return true;
    return false;
}

/* ---------- the API handed to mods ---------- */
static void api_set_color(uint8_t pal)               { gfx_SetColor(pal); }
static void api_fill_rect(int x, int y, int w, int h){ gfx_FillRectangle(x, y, w, h); }
static void api_rect(int x, int y, int w, int h)     { gfx_Rectangle(x, y, w, h); }
static void api_hline(int x, int y, int len)          { gfx_HorizLine(x, y, len); }
static void api_vline(int x, int y, int len)         { gfx_VertLine(x, y, len); }
static void api_fill_circle(int x, int y, int r)     { gfx_FillCircle(x, y, r); }

static void api_set_text_color(uint8_t pal)
{
    ui_text_color(pal);
    gfx_SetTextBGColor(255);
    gfx_SetTextTransparentColor(255);
}

static void api_print(const char *text, int x, int y) { ui_print(text, x, y); }
static int  api_text_width(const char *text)          { return ui_text_width(text); }

static int api_var_read(const char *name, void *buf, int max)
{
    uint8_t h = ti_Open(name, "r");
    int size;

    if (!h) return -1;
    size = (int)ti_GetSize(h);
    if (size > max) size = max;
    if (size > 0 && (int)ti_Read(buf, 1, (size_t)size, h) != size) size = -1;
    ti_Close(h);
    return size;
}

static bool api_var_write(const char *name, const void *buf, int size)
{
    uint8_t h = ti_Open(name, "w");

    if (!h) return false;
    if (size > 0 && (int)ti_Write(buf, 1, (size_t)size, h) != size) {
        ti_Close(h);
        return false;
    }
    ti_Close(h);
    return true;
}

static void api_var_archive(const char *name)
{
    uint8_t h = ti_Open(name, "r");
    if (!h) return;
    ti_SetArchiveStatus(true, h);
    ti_Close(h);
}

static bool api_register_app(const atmos_app_t *app)
{
    if (!app || !app->title || !app->draw || registered_count >= MODS_MAX_APPS)
        return false;
    /* A pointer outside every loaded mod is unrelocated (still its offset from
     * address 0): the shell would call or read through it later and NMI-reset.
     * Reject the app so the mod - and the calculator - survives the bad build. */
    if (!ptr_in_mods(app->title) || !ptr_in_mods(app->draw) ||
        (app->open   && !ptr_in_mods(app->open))   ||
        (app->click  && !ptr_in_mods(app->click))  ||
        (app->hover  && !ptr_in_mods(app->hover))  ||
        (app->tick   && !ptr_in_mods(app->tick))   ||
        (app->keys   && !ptr_in_mods(app->keys))   ||
        (app->press  && !ptr_in_mods(app->press))  ||
        (app->cancel && !ptr_in_mods(app->cancel))) {
        rejected_count++;
        return false;
    }
    registered[registered_count++] = *app;      /* copied: the mod may keep its struct anywhere */
    return true;
}

/* A mod theme (atmos_theme_t) is the same layout as settings_theme_t; only the name is
 * separate there, so the fields are copied one by one. The theme struct is copied into
 * settings.c's registry: the mod may keep it anywhere (or unload). */
static int api_register_theme(const atmos_theme_t *theme)
{
    settings_theme_t t;

    if (!theme || !theme->name) return -1;
    t.top.r     = theme->top.r;     t.top.g     = theme->top.g;     t.top.b     = theme->top.b;
    t.bottom.r  = theme->bottom.r; t.bottom.g  = theme->bottom.g; t.bottom.b  = theme->bottom.b;
    t.bar.r     = theme->bar.r;    t.bar.g     = theme->bar.g;    t.bar.b     = theme->bar.b;
    t.bar_line.r= theme->bar_line.r; t.bar_line.g= theme->bar_line.g; t.bar_line.b= theme->bar_line.b;
    t.dock.r    = theme->dock.r;   t.dock.g    = theme->dock.g;   t.dock.b    = theme->dock.b;
    t.accent.r  = theme->accent.r; t.accent.g  = theme->accent.g; t.accent.b  = theme->accent.b;
    return settings_register_theme(theme->name, &t);
}

/* A mod applies a theme (its own registered one, or any built-in) right away. The
 * switch is saved like a Settings change, and settings_take_changed() makes main.c
 * re-apply the palette and repaint the same frame - no restart needed. */
static bool api_apply_theme(int index)
{
    return settings_select_theme(index);
}

static void api_request_redraw(void) { redraw_requested = true; }
static void api_close_window(void)   { close_requested = true; }

static const atmos_api_t api = {
    ATMOS_API_VERSION,
    304, 186,                                   /* content area; matches WIN_W / WIN_H - TITLE_H */
    api_set_color, api_fill_rect, api_rect, api_hline, api_vline, api_fill_circle,
    api_set_text_color, api_print, api_text_width,
    (volatile uint16_t *)0xF50010,              /* kb_Data */
    api_var_read, api_var_write, api_var_archive,
    api_register_app, api_register_theme, api_request_redraw, api_close_window,
    api_apply_theme                     /* appended last: mods built before it never read it */
};

/* ---------- crash guard ----------
 * A mod that crashes while loading (or during the first frame) would otherwise make atmOS
 * unlaunchable. Before each mod loads, the shell records which mods are in play in the RAM
 * AppVar ATMMODG; mods_boot_ok() deletes it once the first frame is on screen. If ATMMODG is
 * still there at the next start, the previous start crashed: the culprit (the mod that was
 * loading, or every loaded mod if the crash came after loading) goes into the skip list
 * ATMMODX and is not loaded again. A rebuilt mod has a different size, so it is tried again;
 * to retry an unchanged mod, delete ATMMODX from the Files app. */
#define GUARD_VAR  "ATMMODG"
#define SKIP_VAR   "ATMMODX"
#define REC_SIZE   12                   /* name (9) + size (3) */
#define MAX_SKIP   8

static uint8_t skip_n;
static uint8_t skip[MAX_SKIP][REC_SIZE];
static uint8_t guard_n;
static uint8_t guard[MODS_MAX_FOUND][REC_SIZE];
static uint8_t skipped_count;

static void make_rec(uint8_t *rec, const char *name, size_t size)
{
    memset(rec, 0, REC_SIZE);
    strncpy((char *)rec, name, 8);
    rec[9]  = (uint8_t)size;
    rec[10] = (uint8_t)(size >> 8);
    rec[11] = (uint8_t)(size >> 16);
}

static bool in_skip(const uint8_t *rec)
{
    uint8_t i;
    for (i = 0; i < skip_n; i++)
        if (memcmp(skip[i], rec, REC_SIZE) == 0) return true;
    return false;
}

static void add_skip(const uint8_t *rec)
{
    if (!in_skip(rec) && skip_n < MAX_SKIP)
        memcpy(skip[skip_n++], rec, REC_SIZE);
}

static void save_skip(void)
{
    uint8_t buf[1 + MAX_SKIP * REC_SIZE];
    buf[0] = skip_n;
    memcpy(buf + 1, skip, (size_t)skip_n * REC_SIZE);
    api_var_write(SKIP_VAR, buf, 1 + skip_n * REC_SIZE);
}

static void write_guard(uint8_t phase)
{
    uint8_t buf[2 + MODS_MAX_FOUND * REC_SIZE];
    buf[0] = phase;                     /* 0 = loading (last entry is current), 1 = running */
    buf[1] = guard_n;
    memcpy(buf + 2, guard, (size_t)guard_n * REC_SIZE);
    api_var_write(GUARD_VAR, buf, 2 + guard_n * REC_SIZE);
}

/* Reads the skip list and, if the last start never finished, blames and records the mods. */
static void recover_from_crash(void)
{
    uint8_t buf[2 + MODS_MAX_FOUND * REC_SIZE];
    uint8_t sbuf[1 + MAX_SKIP * REC_SIZE];
    int n = api_var_read(SKIP_VAR, sbuf, sizeof sbuf);
    uint8_t i;

    skip_n = 0;
    if (n >= 1 && sbuf[0] <= MAX_SKIP && n == 1 + sbuf[0] * REC_SIZE) {
        skip_n = sbuf[0];
        memcpy(skip, sbuf + 1, (size_t)skip_n * REC_SIZE);
    }

    n = api_var_read(GUARD_VAR, buf, sizeof buf);
    if (n < 0) return;                              /* last start finished normally */
    if (n >= 2 && buf[1] <= MODS_MAX_FOUND && n == 2 + buf[1] * REC_SIZE && buf[1] > 0) {
        if (buf[0] == 0) {
            add_skip(buf + 2 + (buf[1] - 1) * REC_SIZE);
        } else {
            for (i = 0; i < buf[1]; i++)
                add_skip(buf + 2 + i * REC_SIZE);
        }
        save_skip();
    }
    ti_Delete(GUARD_VAR);
}

void mods_boot_ok(void)
{
    if (guard_n) ti_Delete(GUARD_VAR);
    guard_n = 0;
}

int mods_skipped_count(void)
{
    return skipped_count;
}

int mods_rejected_count(void)
{
    return rejected_count;              /* apps dropped for pointers outside mod memory */
}

static int var_size(const char *name)
{
    uint8_t h = ti_Open(name, "r");
    int size;
    if (!h) return -1;
    size = (int)ti_GetSize(h);
    ti_Close(h);
    return size;
}

/* ---------- loading ---------- */
#define RELOC_CHUNK 32

/* Loads one mod AppVar: allocate, copy the image, relocate, call mod_main. */
static void load_mod(const char *name)
{
    atmos_mod_header_t hdr;
    uint16_t offs[RELOC_CHUNK];
    uint8_t h = ti_Open(name, "r");
    uint8_t *mem = NULL;
    size_t size, image, mem_size, entry, count, done, n, i;
    int before;

    if (!h) return;
    size = ti_GetSize(h);
    if (size < sizeof(hdr) ||
        ti_Read(&hdr, 1, sizeof(hdr), h) != sizeof(hdr) ||
        memcmp(hdr.signature, ATMOS_MOD_SIGNATURE, ATMOS_MOD_SIG_LEN) != 0 ||
        hdr.api != ATMOS_API_VERSION)               /* not a mod, or for another API */
        goto bad;

    entry    = (size_t)(uintptr_t)hdr.entry;
    image    = (size_t)(uintptr_t)hdr.image_end;
    mem_size = (size_t)(uintptr_t)hdr.bss_end;
    count    = hdr.reloc_count;
    if (image < sizeof(hdr) || mem_size < image || mem_size > MODS_MAX_MEM ||
        size != image + count * 2 ||                /* truncated or padded */
        entry < sizeof(hdr) || entry >= image)
        goto bad;
    for (i = 0; i < ATMOS_MOD_NAME_MAX && hdr.name[i]; i++)
        if (hdr.name[i] < ' ' || hdr.name[i] > '~') goto bad;     /* garbage header */

    mem = (uint8_t *)malloc(mem_size);
    if (!mem) goto bad;                             /* out of memory: skip this mod */
    /* Never copy a mod across the app RAM / OS stack zone: a write to the stack limit
     * (0xD19881) resets the calculator. Skip the mod instead. */
    if ((uintptr_t)mem < 0xD1A881u && (uintptr_t)mem + mem_size > 0xD1787Cu)
        goto bad;

    ti_Rewind(h);
    if (ti_Read(mem, 1, image, h) != image) goto bad;
    memset(mem + image, 0, mem_size - image);       /* .bss */

    /* Every recorded 24-bit address was linked as an offset from 0; add the load address. */
    for (done = 0; done < count; done += n) {
        n = count - done;
        if (n > RELOC_CHUNK) n = RELOC_CHUNK;
        if (ti_Read(offs, 2, n, h) != n) goto bad;
        for (i = 0; i < n; i++) {
            uint8_t *p;
            uintptr_t v;
            if ((size_t)offs[i] + 3 > image) goto bad;
            p = mem + offs[i];
            v = (uintptr_t)p[0] | ((uintptr_t)p[1] << 8) | ((uintptr_t)p[2] << 16);
            if (v > mem_size) goto bad;                 /* not an address inside this mod: the table is wrong */
            v += (uintptr_t)mem;
            p[0] = (uint8_t)v;
            p[1] = (uint8_t)(v >> 8);
            p[2] = (uint8_t)(v >> 16);
        }
    }
    ti_Close(h);

    NOTE_STACK();
    before = registered_count;
#ifdef MODS_SKIP_ENTRY
    /* TEMP DIAGNOSTIC: the image was read and relocated, but mod_main is never
     * called, so no app registers and no mod code runs. The shell must boot fine
     * this way if (and only if) the crash is inside the mod's own code. */
    (void)entry;
    (void)before;
    free(mem);
#else
    if (!entry_allowed) {                          /* OS stack in use: load + relocate, but never run */
        free(mem);
        return;
    }
    /* Register this mod's memory range first: while mod_main runs,
     * api_register_app checks that every pointer it is handed lies inside
     * a loaded mod (see ptr_in_mods above). */
    if (mod_range_n < MODS_MAX_FOUND) {
        mod_range[mod_range_n].base = (const char *)mem;
        mod_range[mod_range_n].end  = (const char *)mem + mem_size;
        mod_range_n++;
    }
    if (!MODS_CALL_ENTRY((uintptr_t)mem + entry, &api)) {
        registered_count = (uint8_t)before;         /* the mod declined: forget its apps */
        if (mod_range_n && mod_range[mod_range_n - 1].base == (const char *)mem)
            mod_range_n--;                          /* ... and its memory range */
        free(mem);
    }
#endif
    return;

bad:
    ti_Close(h);
    free(mem);                                      /* free(NULL) is fine */
}

void mods_load_all(void)
{
    char names[MODS_MAX_FOUND][9];
    void *vat = NULL;
    char *found;
    int count = 0, i;

    registered_count = 0;
    skipped_count = 0;
    guard_n = 0;
    mod_range_n = 0;
    rejected_count = 0;
    recover_from_crash();                           /* blame + skip mods behind a crashed start */

    /* Collect the names first: loading opens files, which would disturb the search. */
    while (count < MODS_MAX_FOUND &&
           (found = ti_DetectVar(&vat, ATMOS_MOD_SIGNATURE, OS_TYPE_APPVAR)) != NULL) {
        strncpy(names[count], found, 8);
        names[count][8] = '\0';
        count++;
    }
    for (i = 0; i < count; i++) {
        uint8_t rec[REC_SIZE];
        int size = var_size(names[i]);

        if (size < 0) continue;
        make_rec(rec, names[i], (size_t)size);
        if (in_skip(rec)) { skipped_count++; continue; }
        memcpy(guard[guard_n++], rec, REC_SIZE);
        write_guard(0);                             /* if we crash inside load_mod, this one is blamed */
        load_mod(names[i]);
    }
    if (guard_n) write_guard(1);                    /* loaded; main calls mods_boot_ok after frame 1 */
}

int mods_app_count(void)
{
    return registered_count;
}

const atmos_app_t *mods_app(int index)
{
    return (index >= 0 && index < registered_count) ? &registered[index] : NULL;
}

bool mods_take_redraw(void)
{
    bool r = redraw_requested;
    redraw_requested = false;
    return r;
}

bool mods_take_close(void)
{
    bool r = close_requested;
    close_requested = false;
    return r;
}