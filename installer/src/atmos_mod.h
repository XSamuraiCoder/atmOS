#ifndef ATMOS_MOD_H
#define ATMOS_MOD_H

/* atmOS mod API (version 1)
 *
 * A mod is an AppVar (any name) that holds relocatable machine code. atmOS finds mods by content:
 * an AppVar whose data starts with the 32-byte header below, which begins with the text
 * "atmOS mod: " followed by the mod's name. At startup atmOS allocates memory for each valid mod,
 * copies it in, adds the load address to every address recorded in the mod's relocation table, and
 * calls
 *
 *      bool mod_main(const atmos_api_t *api);
 *
 * Use the api to register an app (a dock icon plus a window). Return false to say "I did not
 * load"; the mod's memory is then freed. Mods run inside the shell with full access, so a bug in
 * a mod can crash atmOS. Declare the header with ATMOS_MOD_HEADER and build with the mod Makefile
 * (it links the mod at address 0 and appends the relocation table).
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ATMOS_API_VERSION 1

/* ---- mod file: 32-byte header, the image, then the relocation table ----
 * The mod is linked at address 0, so the pointers below are plain offsets from the start of the
 * image (pointers are 3 bytes on the eZ80, which makes the struct exactly 32 bytes). The mod
 * build appends reloc_count 16-bit offsets after the image; each one is the position of a 24-bit
 * address in the image that atmOS adjusts by the load address. File size = image_end + 2 *
 * reloc_count. */
#define ATMOS_MOD_SIGNATURE "atmOS mod: "
#define ATMOS_MOD_SIG_LEN   11
#define ATMOS_MOD_NAME_MAX  8

typedef struct atmos_mod_header {
    char        signature[ATMOS_MOD_SIG_LEN];       /* "atmOS mod: " (no terminator) */
    char        name[ATMOS_MOD_NAME_MAX + 1];       /* mod name, up to 8 characters + 0 */
    uint8_t     api;                                /* ATMOS_API_VERSION */
    const void *entry;                              /* mod_main: offset from the image start */
    const void *image_end;                          /* size of the loaded image in bytes */
    const void *bss_end;                            /* image + .bss: memory the mod needs */
    uint16_t    reloc_count;                        /* number of relocations (set by the build) */
} atmos_mod_header_t;

#ifdef __ez80__
_Static_assert(sizeof(atmos_mod_header_t) == 32, "atmos_mod_header_t must be 32 bytes");
#endif

/* Put this once in the mod, after mod_main is declared. mod_image_end and mod_bss_end are defined
 * by the mod linker script. The header lands in .modhdr, which the script places first. */
#define ATMOS_MOD_HEADER(mod_name, entry_fn)                                                  \
    extern char mod_image_end[], mod_bss_end[];                                               \
    __attribute__((section(".modhdr"), used))                                                 \
    const atmos_mod_header_t atmos_mod_header = {                                             \
        ATMOS_MOD_SIGNATURE, mod_name, ATMOS_API_VERSION,                                     \
        (const void *)(entry_fn), (const void *)mod_image_end,                                \
        (const void *)mod_bss_end, 0 }

/* Palette entries the shell sets up; pass these to set_color / set_text_color. */
enum {
    ATMOS_PAL_BAR      = 16,   /* light surface (buttons, title bars) */
    ATMOS_PAL_BAR_LINE = 17,   /* window body color */
    ATMOS_PAL_DOCK     = 18,
    ATMOS_PAL_TEXT     = 19,   /* dark text */
    ATMOS_PAL_LABEL    = 20,   /* white */
    ATMOS_PAL_ACCENT   = 21,   /* theme accent */
    ATMOS_PAL_RED = 22, ATMOS_PAL_YELLOW = 23, ATMOS_PAL_GREEN = 24, ATMOS_PAL_BLUE = 25,
    ATMOS_PAL_PURPLE = 26, ATMOS_PAL_CYAN = 27, ATMOS_PAL_ORANGE = 28, ATMOS_PAL_GRAY = 29
};

/* kb[row] bits, same layout as keypadc's kb_Data (the shell scans the keypad every frame). */
#define ATMOS_KB_ENTER_ROW 6
#define ATMOS_KB_ENTER     (1u << 0)
#define ATMOS_KB_2ND_ROW   1
#define ATMOS_KB_2ND       (1u << 5)
#define ATMOS_KB_DEL_ROW   1
#define ATMOS_KB_DEL       (1u << 7)
#define ATMOS_KB_ARROW_ROW 7   /* bit 0 down, 1 left, 2 right, 3 up */
#define ATMOS_KB_DOWN  (1u << 0)
#define ATMOS_KB_LEFT  (1u << 1)
#define ATMOS_KB_RIGHT (1u << 2)
#define ATMOS_KB_UP    (1u << 3)

/* A theme preset a mod adds at runtime. It appears in Settings > Look > Theme after the
 * built-in presets, and register_theme returns its index: pass that index to apply_theme to
 * make it active right away (no restart needed). Registering the same name twice returns the
 * existing index. Colors match the shell's theme struct: bars, docks and window bodies
 * stay light, because the shell's text color is fixed and dark. */
typedef struct atmos_rgb { uint8_t r, g, b; } atmos_rgb_t;

typedef struct atmos_theme {
    const char *name;                  /* Settings dropdown label (up to 15 characters kept) */
    atmos_rgb_t top, bottom;          /* wallpaper gradient */
    atmos_rgb_t bar, bar_line;         /* menu bar / window body (keep light) */
    atmos_rgb_t dock;                  /* dock (keep light) */
    atmos_rgb_t accent;                /* theme accent */
} atmos_theme_t;

/* An app is a dock icon plus a window. Every hook is optional except draw.
 * Coordinates handed to draw() are the window's content origin on the screen;
 * click / hover coordinates are relative to that origin. */
typedef struct atmos_app {
    const char *title;                  /* window title and dock label (max 8 chars in the dock) */
    uint8_t     color;                  /* dock icon color, 0..7: red yellow green blue purple cyan orange gray */
    void (*open)(void);                 /* the window just opened */
    void (*draw)(int x, int y);         /* repaint (clipped by the shell to what changed) */
    void (*click)(int x, int y);        /* Enter / 2nd over the window */
    bool (*hover)(int x, int y);        /* pointer moved over the window; true = redraw */
    bool (*tick)(void);                 /* once per frame while open; true = redraw */
    bool (*keys)(void);                 /* arrows / typing; true = used them (the pointer stays) */
    bool (*press)(void);                /* Enter / 2nd pressed; true = handled (no click) */
    bool (*cancel)(void);               /* Clear pressed; true = handled (window stays open) */
} atmos_app_t;

typedef struct atmos_api {
    uint8_t version;                    /* ATMOS_API_VERSION */
    int content_w, content_h;           /* size of an app window's content area */

    /* drawing; x, y are screen coordinates (use the origin passed to draw) */
    void (*set_color)(uint8_t pal);
    void (*fill_rect)(int x, int y, int w, int h);
    void (*rect)(int x, int y, int w, int h);
    void (*hline)(int x, int y, int len);
    void (*vline)(int x, int y, int len);
    void (*fill_circle)(int x, int y, int r);
    void (*set_text_color)(uint8_t pal);
    void (*print)(const char *text, int x, int y);   /* y = top of an 8 px line; uses the chosen font */
    int  (*text_width)(const char *text);

    /* input */
    volatile uint16_t *kb;              /* kb[0..7], scanned every frame */

    /* storage: AppVars in RAM. read returns the size or -1; write replaces the AppVar. */
    int  (*var_read)(const char *name, void *buf, int max);
    bool (*var_write)(const char *name, const void *buf, int size);
    void (*var_archive)(const char *name);           /* move to flash (can pause for a moment) */

    /* shell */
    bool (*register_app)(const atmos_app_t *app);    /* call from mod_main; the struct is copied */
    int  (*register_theme)(const atmos_theme_t *theme);  /* theme index (copied), or -1 when full */
    void (*request_redraw)(void);                    /* repaint the window on the next frame */
    void (*close_window)(void);                      /* close the window on the next frame */
    bool (*apply_theme)(int index);                  /* appended last (v1 keeps its version):
                                                     * make theme[index] active immediately -
                                                     * saved in SHLSET and applied the same
                                                     * frame; false when index is invalid */
} atmos_api_t;

typedef bool (*atmos_mod_main_t)(const atmos_api_t *api);

#endif