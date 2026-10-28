#ifndef ATMOS_INSTALLER_CONFIG_H
#define ATMOS_INSTALLER_CONFIG_H

/* Everything that must stay in sync between the app packaging (the tool that splits
 * atmOS.8xk into AppVars) and this installer lives here. */

/* The app image arrives split into numbered AppVars: "ATMOS0", "ATMOS1", ... The
 * last AppVar is short (its size differs from APPVAR_SPLIT_SIZE). 8-character name
 * limit on the calculator: prefix + digits must fit. */
#define APPVAR_PREFIX "atmOS"

/* Bytes per AppVar, except the last one. Must match the packaging tool. */
#ifndef APPVAR_SPLIT_SIZE
#define APPVAR_SPLIT_SIZE 0x7F00
#endif

/* The app's name (as it appears in APPS) is read from the first AppVar at
 * data + APP_NAME_OFFSET (packaging format: 256-byte padding, then the app
 * header with the name field). */
#define APP_NAME_OFFSET (256 + 3)

#endif