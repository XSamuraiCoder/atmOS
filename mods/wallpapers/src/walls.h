#ifndef WALLS_H
#define WALLS_H

#include <stdint.h>
#include "atmos_mod.h"

#define WALL_W           160           /* stored size; drawn at 2x */
#define WALL_H           120
#define WALL_COLORS      16            /* ATMOS_PAL_WALL(0..15) */
#define WALL_IMAGE_COUNT 3

/* A picture, run-length encoded: every run is one byte, (length - 1) << 4 | color, runs stop
 * at row ends, and rows[] holds each stored row's first run. */
typedef struct {
    const char        *name;
    const atmos_rgb_t *palette;        /* WALL_COLORS entries */
    const uint16_t    *rows;           /* WALL_H offsets into runs */
    const uint8_t     *runs;
} wall_image_t;

extern const wall_image_t wall_images[WALL_IMAGE_COUNT];   /* wallpapers_data.c (generated) */

#endif
