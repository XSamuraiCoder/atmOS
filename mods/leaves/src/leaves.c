/* leaves.c - "Leaves": an animated atmOS wallpaper with its own theme (mod API 1.2).
 *
 * An autumn dusk (sky, low sun, two rows of hills with trees, ground) with 12 leaves that rock
 * down like real ones. draw() paints any region; tick() moves the leaves and reports only the
 * rectangles that changed. Also registers an "Autumn" theme paired with the wallpaper.
 * Mods link no C library: no memcpy / memset here. */
#include "atmos_mod.h"

#define SCREEN_W    320
#define SCREEN_H    240
#define SCREEN_SIZE (SCREEN_W * SCREEN_H)
#define LEAVES      12
#define LEAF_S      11
#define LEAF_FRAMES 8
#define SKY_BANDS   12
#define BAND_H      15
#define GROUND_Y    214

enum {
    COL_SKY0 = 0,
    COL_SUN = 12, COL_FAR = 13, COL_NEAR = 14, COL_GROUND = 15,
    COL_LEAF0 = 16,
    COL_COUNT = 24
};

static const atmos_rgb_t palette[COL_COUNT] = {
    {34,22,66}, {48,28,78}, {66,34,88}, {88,40,94},
    {112,48,96}, {138,58,94}, {164,70,88}, {190,86,80},
    {212,104,72}, {230,126,70}, {242,150,76}, {250,176,92},
    {255,220,150}, {128,62,86}, {78,36,62}, {44,24,38},
    {214,58,36}, {140,30,24}, {240,128,34}, {170,70,20},
    {246,196,58}, {178,120,24}, {168,96,52}, {100,52,28}
};

/* Background cache. Sky, sun, hills and trees never change, but drawing them (bands, per-column
 * hills, a dozen circles per tree) behind every leaf is most of the work. A full-screen copy
 * would be 76.8 KB - a mod gets 16 KB - so after the first full-screen draw, before any leaf is
 * drawn, the finished background is read back from the screen and kept as runs (color, length)
 * per row, a few KB. From then on a repaint is a few byte fills per row. If the scene ever needs
 * more runs than fit, the cache is dropped and drawing stays the slow way. */
#define CACHE_BYTES 4096                /* the scene needs ~2.5 KB */
static uint8_t  cache[CACHE_BYTES];
static uint16_t row_start[SCREEN_H];
static bool     cached, cache_failed;

static const uint16_t leaf_body[LEAF_FRAMES][LEAF_S] = {
    {0x000,0x000,0x020,0x1FC,0x3FE,0x7FF,0x3FE,0x1FC,0x020,0x000,0x000},
    {0x000,0x000,0x038,0x0FE,0x1FE,0x3FE,0x3FC,0x7F8,0x0E0,0x000,0x000},
    {0x000,0x000,0x07C,0x0FC,0x1FC,0x1FC,0x1FC,0x1F8,0x1F0,0x200,0x400},
    {0x000,0x038,0x078,0x0FC,0x0FC,0x1FC,0x1F8,0x1F8,0x0F0,0x0E0,0x080},
    {0x020,0x070,0x0F8,0x0F8,0x0F8,0x1FC,0x0F8,0x0F8,0x0F8,0x070,0x020},
    {0x000,0x0E0,0x0F0,0x1F8,0x1F8,0x1FC,0x0FC,0x0FC,0x078,0x038,0x008},
    {0x000,0x000,0x1F0,0x1F8,0x1FC,0x1FC,0x1FC,0x0FC,0x07C,0x002,0x001},
    {0x000,0x000,0x0E0,0x3F8,0x3FC,0x3FE,0x1FE,0x0FF,0x038,0x000,0x000}
};

static const uint16_t leaf_vein[LEAF_FRAMES][LEAF_S] = {
    {0x000,0x000,0x000,0x000,0x000,0x1FF,0x000,0x000,0x000,0x000,0x000},
    {0x000,0x000,0x000,0x002,0x00C,0x070,0x180,0x000,0x000,0x000,0x000},
    {0x000,0x000,0x004,0x008,0x010,0x020,0x040,0x080,0x000,0x000,0x000},
    {0x000,0x008,0x010,0x010,0x020,0x020,0x020,0x040,0x040,0x000,0x000},
    {0x020,0x020,0x020,0x020,0x020,0x020,0x020,0x020,0x020,0x000,0x000},
    {0x000,0x080,0x040,0x040,0x020,0x020,0x020,0x010,0x010,0x000,0x000},
    {0x000,0x000,0x100,0x080,0x040,0x020,0x010,0x008,0x000,0x000,0x000},
    {0x000,0x000,0x000,0x200,0x180,0x070,0x00C,0x000,0x000,0x000,0x000}
};

static const int8_t sin64[64] = {
    0,12,25,37,49,60,71,81,90,98,106,112,117,122,125,126,
    127,126,125,122,117,112,106,98,90,81,71,60,49,37,25,12,
    0,-12,-25,-37,-49,-60,-71,-81,-90,-98,-106,-112,-117,-122,-125,-126,
    -127,-126,-125,-122,-117,-112,-106,-98,-90,-81,-71,-60,-49,-37,-25,-12
};

static const atmos_api_t *api;
static bool ready;
static uint16_t seed = 0x5EED;

static int wave(int phase, int amp)
{
    return sin64[phase & 63] * amp / 127;
}

static int far_hill(int x)
{
    return 148 + wave(x / 6 + 9, 10) + wave(x / 2, 3);
}

static int near_hill(int x)
{
    return 180 + wave(x / 8 + 40, 13) + wave(x / 3 + 7, 4);
}

/* How a leaf falls (tuned for 60 steps a second, one per shell frame):
 *  - FLUTTER: it rocks like a pendulum around x - level at the bottom of a swing, tipped up at
 *    the ends - and glides: fast through the middle, stalling (even lifting a little) at the ends.
 *  - TUMBLE:  now and then it flips end over end instead, falling a bit faster and sliding sideways.
 *  - REST:    it lands on the hillside, lies there a few seconds, then a new leaf starts at the top.
 * Light leaves swing wider, fall slower and catch more wind. The wind itself drifts and gusts. */
enum { FLUTTER, TUMBLE, REST };

typedef struct {
    int24_t  x, y;                     /* 1/256 px; x = the swing's center while fluttering */
    uint16_t fall;                     /* fall per step at the middle of a swing, 1/256 px */
    uint16_t phase;                    /* 8.8: swing (FLUTTER) or spin angle (TUMBLE) */
    uint16_t timer;                    /* steps until a mode change / until it leaves the ground */
    uint8_t  step, amp, kind, weight, mode;   /* weight 0 light .. 3 heavy */
    int8_t   spin;                     /* tumbling direction */
} leaf_t;

#define PH(l) ((uint8_t)((l)->phase >> 8))

static leaf_t leaf[LEAVES];
static int      wind, wind_target;     /* 1/256 px per step, + = to the right */
static uint16_t gust_timer;

static uint8_t rnd(void)
{
    seed = (uint16_t)(seed * 25173u + 13849u);
    return (uint8_t)(seed >> 8);
}

static void spawn(leaf_t *l, bool anywhere)
{
    l->weight = (uint8_t)(rnd() & 3);
    l->x = (int24_t)((rnd() * (SCREEN_W + 60) / 255 - 30) * 256);
    l->y = anywhere ? (int24_t)((rnd() * 150 / 255) << 8)         /* above the hills */
                    : (int24_t)(-LEAF_S * 256);
    l->fall = (uint16_t)(110 + l->weight * 35 + rnd() % 50);
    l->phase = (uint16_t)(rnd() << 8);
    l->step = (uint8_t)(3 + rnd() % 3);
    l->amp = (uint8_t)(12 + (3 - l->weight) * 3 + rnd() % 8);      /* light ones swing wider */
    l->kind = (uint8_t)(rnd() & 3);
    l->mode = (rnd() & 3) == 0 ? TUMBLE : FLUTTER;
    l->spin = (int8_t)(rnd() & 1 ? 1 : -1);
    l->timer = (uint16_t)(60 + rnd());
}

static void init(void)
{
    int i;
    if (ready) return;
    ready = true;
    wind = wind_target = 10;
    gust_timer = 120;
    for (i = 0; i < LEAVES; i++)
        spawn(&leaf[i], true);
}

static int swing_cos(const leaf_t *l)
{
    return sin64[((PH(l) >> 2) + 16) & 63];
}

static int leaf_sx(const leaf_t *l)
{
    int x = (int)(l->x >> 8);
    return l->mode == FLUTTER ? x + wave(PH(l) >> 2, l->amp) : x;
}

static int leaf_sy(const leaf_t *l)
{
    return (int)(l->y >> 8);
}

static int leaf_frame(const leaf_t *l)
{
    int s;
    if (l->mode == REST) return 0;                       /* lying flat */
    if (l->mode == TUMBLE) return PH(l) >> 5;            /* 0..7: half a turn per 256 */
    s = -sin64[(PH(l) >> 2) & 63];                       /* tilt with the swing: -2..2 */
    return ((s * 2 + (s >= 0 ? 63 : -63)) / 127) & (LEAF_FRAMES - 1);
}

/* the wind eases toward a target that changes every few seconds; sometimes a gust */
static void step_wind(void)
{
    int d;
    if (gust_timer-- == 0) {
        gust_timer = (uint16_t)(120 + rnd() * 2);       /* 2 .. 10 s */
        wind_target = -8 + rnd() % 36;                   /* mostly to the right, sometimes back */
        if (rnd() < 40) wind_target += 48;               /* a gust */
    }
    d = wind_target - wind;
    wind += d / 24 ? d / 24 : (d > 0) - (d < 0);
}

static void draw_leaf(uint8_t *buf, const leaf_t *l,
                      int x0, int y0, int x1, int y1)
{
    int sx = leaf_sx(l);
    int sy = leaf_sy(l);
    int f = leaf_frame(l);
    int r, c;

    uint8_t light = ATMOS_PAL_WALL(COL_LEAF0 + 2 * l->kind);
    uint8_t dark = ATMOS_PAL_WALL(COL_LEAF0 + 2 * l->kind + 1);

    for (r = 0; r < LEAF_S; r++) {
        int y = sy + r;
        uint16_t body = leaf_body[f][r];
        uint16_t vein = leaf_vein[f][r];

        if (y < y0 || y >= y1 || !body) continue;

        for (c = 0; c < LEAF_S; c++) {
            int x = sx + c;

            if (!(body >> c & 1) || x < x0 || x >= x1)
                continue;

            buf[y * SCREEN_W + x] =
                (vein >> c & 1) || c + r > LEAF_S ? dark : light;
        }
    }
}

/* Trees */

typedef struct {
    int16_t x;
    uint8_t r, kind;
} tree_t;

#define TREES 5

static const tree_t trees[TREES] = {
    {34,21,0}, {84,13,2}, {146,9,3},
    {198,12,1}, {288,22,1}
};

static void draw_tree(const tree_t *t, int x0, int y0, int x1, int y1)
{
    int r = t->r;
    int base = near_hill(t->x) + 3;
    int trunk = r * 3 / 2;
    int cy = base - trunk - r / 2;
    int w = r / 5 + 2;

    uint8_t light = ATMOS_PAL_WALL(COL_LEAF0 + 2 * t->kind);
    uint8_t dark = ATMOS_PAL_WALL(COL_LEAF0 + 2 * t->kind + 1);

    if (t->x + r * 2 <= x0 || t->x - r * 2 >= x1 ||
        cy - r * 2 >= y1 || base <= y0)
        return;

    api->set_color(ATMOS_PAL_WALL(COL_GROUND));

    api->fill_rect(t->x - w / 2, cy, w, base - cy);
    api->fill_rect(t->x - r / 2, cy + r / 3, r / 2, 2);
    api->fill_rect(t->x, cy + r / 2, r / 2, 2);

    api->set_color(dark);

    api->fill_circle(t->x, cy, r);
    api->fill_circle(t->x - r * 3 / 5, cy + r / 3, r * 2 / 3);
    api->fill_circle(t->x + r * 3 / 5, cy + r / 4, r * 2 / 3);
    api->fill_circle(t->x, cy - r / 2, r * 3 / 5);

    api->set_color(light);

    api->fill_circle(t->x - r / 4, cy - r / 4, r * 3 / 5);
    api->fill_circle(t->x - r * 3 / 5, cy + r / 5, r * 2 / 5);
    api->fill_circle(t->x + r / 5, cy - r * 3 / 5, r * 2 / 5);
}

static void fill(int x0, int y0, int x1, int y1, int col)
{
    if (x1 <= x0 || y1 <= y0) return;

    api->set_color(ATMOS_PAL_WALL(col));
    api->fill_rect(x0, y0, x1 - x0, y1 - y0);
}

/* Draw only the static scenery. */

static void draw_background(int x0, int y0, int x1, int y1)
{
    int i, x;

    for (i = y0 / BAND_H; i < SKY_BANDS && i * BAND_H < y1; i++)
        fill(x0,
             i * BAND_H > y0 ? i * BAND_H : y0,
             x1,
             (i + 1) * BAND_H < y1 ? (i + 1) * BAND_H : y1,
             COL_SKY0 + i);

    if (y1 > SKY_BANDS * BAND_H)
        fill(x0,
             y0 > SKY_BANDS * BAND_H ? y0 : SKY_BANDS * BAND_H,
             x1, y1, COL_FAR);

    if (x1 > 222 && x0 < 258 && y1 > 128 && y0 < 164) {
        api->set_color(ATMOS_PAL_WALL(COL_SUN));
        api->fill_circle(240, 146, 17);
    }

    if (y1 > 126) {
        for (x = x0; x < x1; x++) {
            int f = far_hill(x);
            int n = near_hill(x);

            if (f < SKY_BANDS * BAND_H) {
                api->set_color(ATMOS_PAL_WALL(COL_FAR));
                api->vline(x, f, n - f);
            }

            api->set_color(ATMOS_PAL_WALL(COL_NEAR));
            api->vline(x, n, GROUND_Y - n);
        }
    }

    for (i = 0; i < TREES; i++)
        draw_tree(&trees[i], x0, y0, x1, y1);

    if (y1 > GROUND_Y)
        fill(x0, y0 > GROUND_Y ? y0 : GROUND_Y,
             x1, y1, COL_GROUND);
}

static void build_cache(const uint8_t *buf)
{
    int y, x, n = 0;

    for (y = 0; y < SCREEN_H; y++) {
        const uint8_t *row = buf + y * SCREEN_W;
        row_start[y] = (uint16_t)n;
        for (x = 0; x < SCREEN_W; ) {
            uint8_t c = row[x];
            int len = 1;
            while (x + len < SCREEN_W && len < 255 && row[x + len] == c) len++;
            if (n + 2 > CACHE_BYTES) { cache_failed = true; return; }
            cache[n++] = c;
            cache[n++] = (uint8_t)len;
            x += len;
        }
    }
    cached = true;
}

static void draw_cached(uint8_t *buf, int x0, int y0, int x1, int y1)
{
    int y;

    for (y = y0; y < y1; y++) {
        const uint8_t *run = cache + row_start[y];
        uint8_t *row = buf + y * SCREEN_W;
        int x = 0;
        while (x < x1) {
            uint8_t c = run[0];
            int end = x + run[1];
            run += 2;
            if (end > x0) {
                int from = x > x0 ? x : x0, to = end < x1 ? end : x1;
                uint8_t *p = row + from;
                for (; from < to; from++) *p++ = c;
            }
            x = end;
        }
    }
}

/* the background for this region: from the cache, or drawn (and cached on a full-screen draw) */
static void restore_background(int x0, int y0, int x1, int y1)
{
    if (cached) {
        draw_cached(api->draw_buffer(), x0, y0, x1, y1);
        return;
    }
    draw_background(x0, y0, x1, y1);
    if (!cache_failed && x0 == 0 && y0 == 0 && x1 == SCREEN_W && y1 == SCREEN_H)
        build_cache(api->draw_buffer());           /* the whole background is on screen now */
}

/* Wallpaper draw callback. */

static void draw(int x0, int y0, int x1, int y1)
{
    int i;

    init();

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;

    if (x1 <= x0 || y1 <= y0) return;

    restore_background(x0, y0, x1, y1);

    {
        uint8_t *buf = api->draw_buffer();

        for (i = 0; i < LEAVES; i++) {
            const leaf_t *l = &leaf[i];
            int sx = leaf_sx(l);
            int sy = leaf_sy(l);

            if (sx + LEAF_S > x0 && sx < x1 &&
                sy + LEAF_S > y0 && sy < y1)
                draw_leaf(buf, l, x0, y0, x1, y1);
        }
    }
}

/* Animation update: dirty rectangles only. */

static int tick(int rects[][4], int max)
{
    int i, n = 0;

    init();
    step_wind();
    for (i = 0; i < LEAVES && n < max; i++) {
        leaf_t *l = &leaf[i];
        int ox = leaf_sx(l), oy = leaf_sy(l), of = leaf_frame(l), nx, ny;
        bool respawn = false;

        if (l->mode == REST) {
            if (--l->timer) continue;                    /* lying still: nothing to repaint */
            respawn = true;
        } else {
            int drift = wind * (5 - l->weight) / 4;      /* light leaves catch more wind */

            if (--l->timer == 0) {                       /* now and then: flutter <-> tumble */
                int sx = leaf_sx(l);
                l->mode = l->mode == FLUTTER && (rnd() & 1) ? TUMBLE : FLUTTER;
                l->x = (int24_t)(sx * 256);              /* keep it where it is on screen */
                l->phase = l->mode == FLUTTER ? 0 : l->phase;   /* swing offset 0 at phase 0 */
                l->timer = (uint16_t)(60 + rnd() * 2);
            }
            if (l->mode == FLUTTER) {
                int c = swing_cos(l);
                if (c < 0) c = -c;
                l->phase = (uint16_t)(l->phase + (l->step << 7));
                /* glide: fast through the middle, a slight lift at the ends */
                l->y += (int24_t)((int)l->fall * (c * 160 / 127 - 9) / 256);   /* avg = the old speed */
            } else {                                     /* TUMBLE */
                l->phase = (uint16_t)(l->phase + l->spin * 2048);
                l->y += (int24_t)(l->fall * 100 / 256);
                drift += l->spin * 40;                   /* tumbling leaves slide sideways */
            }
            l->x += drift;

            nx = leaf_sx(l);
            ny = leaf_sy(l);
            if (nx >= -4 && nx < SCREEN_W - LEAF_S + 4) {
                int ground = near_hill(nx + LEAF_S / 2) + 1;   /* hillside under the leaf */
                if (ny + 8 >= ground) {                  /* landed: lie flat on the slope */
                    l->mode = REST;
                    l->x = (int24_t)(nx * 256);
                    l->y = (int24_t)((ground - 8) * 256);
                    l->timer = (uint16_t)(180 + rnd() * 2);    /* 3 .. 11 s */
                }
            }
            if (ny >= SCREEN_H || nx < -40 || nx >= SCREEN_W + 4) respawn = true;
        }
        if (respawn) {
            /* a new leaf from the top. Report only where it was: the union with the new spot
             * would be a full-height strip repainted every time. */
            spawn(l, false);
            nx = ox;
            ny = oy;
        } else {
            nx = leaf_sx(l);
            ny = leaf_sy(l);
            if (ox == nx && oy == ny && of == leaf_frame(l)) continue;
        }
        rects[n][0] = ox < nx ? ox : nx;
        rects[n][1] = oy < ny ? oy : ny;
        rects[n][2] = (ox > nx ? ox : nx) + LEAF_S;
        rects[n][3] = (oy > ny ? oy : ny) + LEAF_S;
        n++;
    }
    return n;
}

/* Module entry point. */

bool mod_main(const atmos_api_t *a)
{
    static const atmos_wallpaper_t wp = {
        "Leaves", palette, COL_COUNT, draw
    };

    static const atmos_theme_t th = {
        "Autumn",
        {40,26,72}, {236,150,80},
        {250,240,230}, {214,190,170},
        {246,232,218}, {214,90,40}
    };

    int wall, theme;

    if (a->version != ATMOS_API_VERSION || a->minor < 2)
        return false;

    api = a;

    wall = api->register_wallpaper(&wp);
    if (wall < 0)
        return false;

    /* a step every shell frame, ~60 a second (the shell halves it while the pointer moves) */
    api->set_wallpaper_anim(wall, tick, 1);

    theme = api->register_theme(&th);
    if (theme >= 0)
        api->set_theme_wallpaper(theme, wall);

    return true;
}

ATMOS_MOD_HEADER("Leaves", mod_main);
