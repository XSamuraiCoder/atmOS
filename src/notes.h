#ifndef SHELL_NOTES_H
#define SHELL_NOTES_H

#include <stdbool.h>
#include <stdint.h>

/* Built-in Notes app. Same hook shape as the Files app in the app_t table. */
void notes_open(void);                      /* call when the window opens */
void notes_draw(int x, int y);
void notes_click(int x, int y);
bool notes_hover(int x, int y);
bool notes_keys(void);                      /* arrows / typing; true = used */
bool notes_press(void);                     /* Enter / 2nd; true = handled */
bool notes_cancel(void);                    /* Clear; true = handled (back to list) */
void notes_flush(void);                     /* save + archive; call before leaving the shell */

/* If a NOTES program is installed, the shell launches it instead of this UI. */
bool notes_find_program(const char **name, uint8_t *var_type, uint8_t *program_kind);

#endif