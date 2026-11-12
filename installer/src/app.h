#include <stddef.h>

void *os_FindAppStart(const char *name);
void *find_last_app();
void fix_relocations(void *app);
bool confirm_delete_vars(void);
void delete_var(const char *name, uint8_t type);
void *find_app(const char *name);   /* FindAppUp walk: skips deleted apps; NULL = not installed */
bool delete_app(const char *name);  /* DeleteApp + garbage collect on exit; false = not found */
