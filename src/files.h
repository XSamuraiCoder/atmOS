#ifndef SHELL_FILES_H
#define SHELL_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FILES_MAX_PINS 6
#define SHELL_APP_NAME "atmOS"   /* installed Flash App name: must match NAME in the Makefile */
#define FILES_TYPE_APP 0x14

typedef struct {
    char name[9];
    uint8_t type;
    uint8_t program_kind;
} files_pin_t;

typedef enum {
    FILES_PIN_OK,
    FILES_PIN_FULL,
    FILES_PIN_STORAGE_ERROR
} files_pin_result_t;

size_t files_get_pins(files_pin_t *pins, size_t capacity);
bool files_is_pinned(const char *name, uint8_t type);
files_pin_result_t files_set_pin(const files_pin_t *pin, bool pinned);
bool files_take_pins_changed(void);
bool files_load_pin_icon(const files_pin_t *pin, uint8_t pixels[16 * 16]);

void files_open(void);
void files_draw(int x, int y);
void files_draw_region(int x, int y, int x0, int y0, int x1, int y1);
void files_click(int x, int y);
bool files_tick(void);
bool files_hover(int x, int y);
bool files_keys(void);
bool files_press(void);
bool files_alpha(void);
bool files_menu_active(void);
bool files_cancel_menu(void);
bool files_take_launch_request(const char **prgm, void **app_header,
                               uint8_t *program_kind, uint8_t *var_type);

#endif