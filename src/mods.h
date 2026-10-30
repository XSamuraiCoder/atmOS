#ifndef SHELL_MODS_H
#define SHELL_MODS_H

#include <stdbool.h>
#include "atmos_mod.h"

/* atmOS mods: relocatable machine-code AppVars. At startup the shell looks for AppVars whose data
 * starts with "atmOS mod: " (see atmos_mod_header_t), allocates memory for each, copies it in,
 * applies the relocation table and calls mod_main. The AppVar name does not matter and there
 * are no fixed addresses. See atmos_mod.h for the mod side. */
#define MODS_MAX_APPS   4
#define MODS_MAX_FOUND  8              /* mod AppVars examined per start */
#define MODS_MAX_MEM    0x4000u        /* 16 KB per mod: image + .bss; the mod Makefile must match */

void mods_load_all(void);              /* call once at startup, after the file system is usable */
int  mods_app_count(void);
const atmos_app_t *mods_app(int index);
bool mods_take_redraw(void);           /* a mod asked for a repaint */
bool mods_take_close(void);            /* a mod asked to close its window */

/* Runtime version of the MODS_SKIP_ENTRY diagnostic: when false, every mod is
 * still found, loaded and relocated, but mod_main is never called. Used when the
 * shell ends up on the OS stack (no malloc'd stack of its own): a mod's stack
 * use is unknown and the OS stack is nearly exhausted. */
void mods_set_entry_allowed(bool allowed);

/* The start survived: call once the first frame is on screen. This deletes the crash
 * guard (ATMMODG). Without it, the next start blames every loaded mod and adds them
 * all to the skip list (ATMMODX) even after a perfectly clean run. */
void mods_boot_ok(void);

/* Apps the shell refused at registration because their pointers did not point inside
 * any loaded mod - i.e. the mod build's relocation table missed them (mods are
 * linked at address 0). Nonzero after a start that dropped a mod's app for that
 * reason; useful next to mods_skipped_count on the boot display. */
int  mods_rejected_count(void);

#endif