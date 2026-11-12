#!/usr/bin/env python3
"""Paints the picture wallpaper mods (WPDUNES, WPLAKE, WPAURORA) and writes their data.

    python tools/gen_wallpapers.py      (from mods/wallpapers)

Every wallpaper is original art drawn here from code (no source images). Each one is rendered
at 320x240, scaled to 160x120 (the shell shows it at 2x) and stored at 8 bits per pixel: every
byte is a palette entry. A picture has ~240 colors: its own 24 (ATMOS_PAL_WALL(0..23), picked
by median cut) plus the shell's fixed 6x6x6 cube (ATMOS_PAL_CUBE), Floyd-Steinberg dithered
over all of them. Each wallpaper is one mod: the Makefile appends data/<name>.bin to the mod
code, and the mod reads it in place from its archived AppVar, so the picture costs no RAM.
Outputs:
    data/<name>.bin       "atmOSwp2", name (16 bytes, zero padded), 24 colors (r, g, b),
                          theme colors (top, bottom, bar, bar_line, dock, accent: r, g, b each),
                          160*120 palette entries
    previews/<name>.png   previews at screen size
Needs Pillow and numpy.
"""
import os
import numpy as np
from PIL import Image

W, H = 320, 240           # render size (screen)
OW, OH = 160, 120         # stored size
COLORS = 24               # the picture's own colors
MAGIC = b"atmOSwp2"

# The theme each picture brings (paired with it): gradient (shown if the user picks Gradient),
# then light bars / dock (the shell's text is dark) and an accent taken from the picture.
#           top             bottom           bar              bar_line         dock             accent
THEMES = {
    "Dunes":  [(38, 22, 74),  (247, 128, 74), (250, 240, 232), (214, 190, 176), (246, 232, 220), (226, 98, 62)],
    "Lake":   [(54, 112, 196), (206, 228, 244), (236, 244, 250), (180, 198, 214), (226, 238, 246), (36, 118, 196)],
    "Aurora": [(6, 10, 32),    (26, 52, 88),    (232, 240, 244), (184, 198, 206), (222, 234, 240), (34, 186, 140)],
}

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rng = np.random.default_rng(84)

yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)


def lerp(a, b, t):
    t = np.clip(t, 0, 1)[..., None]
    return np.asarray(a, np.float32) * (1 - t) + np.asarray(b, np.float32) * t


def vgrad(stops):
    """Vertical gradient through (y_fraction, rgb) stops."""
    img = np.zeros((H, W, 3), np.float32)
    f = yy / (H - 1)
    for (y0, c0), (y1, c1) in zip(stops, stops[1:]):
        m = (f >= y0) & (f <= y1)
        img[m] = lerp(c0, c1, (f[m] - y0) / max(y1 - y0, 1e-6))
    return img


def paint(img, mask, color, alpha=1.0):
    a = (np.clip(mask, 0, 1) * alpha)[..., None]
    img[:] = img * (1 - a) + np.asarray(color, np.float32) * a


def ridge(base, amps, seed):
    """Height per column: base + a few sine octaves with random phases."""
    r = np.random.default_rng(seed)
    x = np.arange(W, dtype=np.float32)
    h = np.full(W, base, np.float32)
    for period, amp in amps:
        h += amp * np.sin(2 * np.pi * x / period + r.uniform(0, 2 * np.pi))
    return h


def below(h):
    """Soft mask of pixels under the per-column height h."""
    return np.clip(yy - h[None, :] + 0.5, 0, 1)


def fractal_ridge(base, rough, seed, levels=7):
    """Jagged mountain line by midpoint displacement."""
    r = np.random.default_rng(seed)
    pts = np.array([base + r.uniform(-rough, rough), base + r.uniform(-rough, rough)])
    amp = rough
    for _ in range(levels):
        mids = (pts[:-1] + pts[1:]) / 2 + r.uniform(-amp, amp, len(pts) - 1)
        out = np.empty(len(pts) * 2 - 1)
        out[0::2], out[1::2] = pts, mids
        pts, amp = out, amp * 0.55
    return np.interp(np.linspace(0, len(pts) - 1, W), np.arange(len(pts)), pts).astype(np.float32)


# ---------------------------------------------------------------- wallpapers
def dunes():
    img = vgrad([(0.0, (38, 22, 74)), (0.35, (150, 52, 110)), (0.58, (247, 128, 74)),
                 (0.70, (255, 196, 120)), (1.0, (255, 196, 120))])
    sx, sy = 205, 112
    d = np.hypot(xx - sx, yy - sy)
    paint(img, np.exp(-(d / 60) ** 2), (255, 170, 110), 0.55)     # glow
    paint(img, 26 - d, (255, 236, 170))                           # sun
    for i, (y, c) in enumerate([(98, (255, 210, 160)), (106, (255, 190, 140))]):
        band = np.clip(1 - np.abs(yy - y - 3 * np.sin(xx / 40 + i)) / 1.5, 0, 1)
        paint(img, band * (np.abs(xx - sx) < 70), c, 0.6)         # thin clouds over the sun
    layers = [
        (150, [(260, 10), (120, 4)], 1, (196, 92, 84), (150, 60, 76)),
        (172, [(300, 12), (140, 5)], 2, (222, 128, 86), (168, 78, 70)),
        (200, [(340, 14), (110, 4)], 3, (238, 160, 98), (186, 98, 72)),
    ]
    for base, amps, seed, lit, shade in layers:
        h = ridge(base, amps, seed)
        slope = np.gradient(h)
        m = below(h)
        depth = np.clip((yy - h[None, :]) / 40, 0, 1)
        col = lerp(lit, shade, depth * 0.6 + np.clip(slope, 0, 1)[None, :] * 0.6)
        img[:] = img * (1 - m[..., None]) + col * m[..., None]
    return img


def lake():
    img = vgrad([(0.0, (54, 112, 196)), (0.42, (150, 196, 236)), (0.55, (206, 228, 244)),
                 (1.0, (206, 228, 244))])
    for cx, cy, s in [(70, 42, 1.0), (230, 30, 0.8), (150, 64, 0.6)]:
        blob = np.exp(-(((xx - cx) / (46 * s)) ** 2 + ((yy - cy) / (9 * s)) ** 2))
        paint(img, blob * 1.6, (246, 250, 255), 0.85)              # clouds
    horizon = 150
    far = fractal_ridge(84, 34, 7)
    near = ridge(132, [(210, 9), (75, 4), (31, 1.5)], 11)
    far = np.minimum(far, horizon); near = np.minimum(near, horizon)
    m = below(far) * (yy < horizon)
    snow = np.clip((104 + 5 * np.sin(xx / 9) + 3 * np.sin(xx / 3.3) - yy) / 2, 0, 1)   # snow line
    paint(img, m, (112, 132, 170))
    paint(img, m * snow, (236, 242, 250))
    shade = np.clip(np.gradient(far), 0, 1)[None, :]
    paint(img, m * shade * 0.5, (84, 100, 140))
    m2 = below(near) * (yy < horizon)
    paint(img, m2, (58, 96, 92))
    paint(img, m2 * np.clip((yy - near[None, :]) / 30, 0, 1), (36, 70, 66))
    # lake: mirrored sky + mountains, darker and bluer
    water = yy >= horizon
    src_y = np.clip(2 * horizon - yy, 0, H - 1).astype(int)
    mirror = img[src_y, xx.astype(int)]
    tint = lerp(mirror, (40, 92, 150), 0.45 + (yy - horizon) / (H - horizon) * 0.3)
    img[water] = tint[water]
    ripple = (np.sin(yy * 1.7 + np.sin(xx / 13) * 2) > 0.92) & water
    paint(img, ripple.astype(np.float32), (190, 220, 245), 0.35)
    # shore with pines on the left
    shore = ridge(176, [(200, 4), (60, 2)], 5)
    shore_mask = below(shore) * (xx < 120 + 30 * np.sin(yy / 17))
    paint(img, shore_mask, (34, 62, 46))
    for tx in [18, 34, 52, 70, 86]:
        th = 46 + (tx * 7) % 18
        top = shore[tx] - th
        for k in range(6):
            y0 = top + k * th / 6
            half = 3 + k * 2.2
            tri = ((yy >= y0) & (yy <= y0 + th / 3) &
                   (np.abs(xx - tx) <= half * (yy - y0) / (th / 3) + 0.5))
            paint(img, tri.astype(np.float32), (22, 48, 36))
    return img


def aurora():
    img = vgrad([(0.0, (6, 10, 32)), (0.5, (14, 30, 66)), (0.85, (26, 52, 88)), (1.0, (26, 52, 88))])
    for k, (base, amp, col, top, height) in enumerate([
            (104, 16, (70, 255, 150), (60, 170, 200), 64),
            (124, 10, (120, 255, 190), (150, 110, 230), 40)]):
        center = base + amp * np.sin(xx / 53 + k * 2.3) + 5 * np.sin(xx / 17 + k)
        up = (center - yy) / height                                 # 0 at the lower edge, 1 at the top
        rays = 0.6 + 0.4 * np.sin(xx / 4.5 + k * 2 + np.sin(xx / 23) * 4) ** 2
        curtain = np.where(yy <= center, np.exp(-up * 2.2), np.exp(-((yy - center) / 2.0) ** 2))
        curtain *= np.where(yy <= center, rays, 1.0)
        color = lerp(col, top, np.clip(up, 0, 1) * 1.4)
        a = np.clip(curtain, 0, 1)[..., None] * 0.85
        img[:] = img * (1 - a) + color * a
    stars = rng.random((H, W)) > 0.9965
    stars &= yy < 150
    paint(img, stars.astype(np.float32), (250, 250, 255))
    big = rng.random((H, W)) > 0.9993
    for y, x in zip(*np.nonzero(big & (yy < 120))):
        paint(img, np.exp(-((xx - x) ** 2 + (yy - y) ** 2) / 1.2), (255, 255, 235))
    hills = ridge(176, [(240, 12), (90, 6), (37, 2)], 21)
    paint(img, below(hills), (8, 18, 30))
    near = ridge(204, [(300, 10), (70, 4)], 23)
    paint(img, below(near), (4, 10, 18))
    return img


WALLPAPERS = [("Dunes", dunes), ("Lake", lake), ("Aurora", aurora)]


# ---------------------------------------------------------------- output
CUBE = np.array([(r * 51, g * 51, b * 51) for r in range(6) for g in range(6) for b in range(6)],
                np.float32)


def slot(i):
    """Palette entry of combined color i: 0..23 own colors, 24.. the cube."""
    if i < 24:
        return i if i < 16 else 246 + i - 16
    return 30 + i - 24


def reduce(img):
    im = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB").resize((OW, OH), Image.LANCZOS)
    own = im.quantize(colors=COLORS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = own.getpalette()[:COLORS * 3]
    pal += [0] * (COLORS * 3 - len(pal))
    own_rgb = np.array(pal, np.float32).reshape(COLORS, 3)
    colors = np.vstack([own_rgb, CUBE])                      # 240 colors
    work = np.array(im, np.float32)
    idx = np.zeros((OH, OW), np.uint8)
    for y in range(OH):                                      # Floyd-Steinberg over all 240
        for x in range(OW):
            old = work[y, x]
            k = int(np.argmin(((colors - old) ** 2).sum(1)))
            idx[y, x] = k
            err = old - colors[k]
            if x + 1 < OW: work[y, x + 1] += err * 7 / 16
            if y + 1 < OH:
                if x > 0: work[y + 1, x - 1] += err * 3 / 16
                work[y + 1, x] += err * 5 / 16
                if x + 1 < OW: work[y + 1, x + 1] += err * 1 / 16
    return idx, own_rgb.astype(np.uint8), colors.astype(np.uint8)


def main():
    os.makedirs(os.path.join(ROOT, "previews"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "data"), exist_ok=True)
    slots = np.array([slot(i) for i in range(COLORS + len(CUBE))], np.uint8)
    for name, fn in WALLPAPERS:
        idx, own, colors = reduce(fn())
        ident = name.lower()
        theme = bytes(c for rgb in THEMES[name] for c in rgb)
        data = (MAGIC + name.encode().ljust(16, b"\0") + own.tobytes() + theme +
                slots[idx].tobytes())
        with open(os.path.join(ROOT, "data", f"{ident}.bin"), "wb") as f:
            f.write(data)
        prev = Image.fromarray(colors[idx], "RGB").resize((W, H), Image.NEAREST)
        prev.save(os.path.join(ROOT, "previews", f"{ident}.png"))
        print(f"data/{ident}.bin, {len(data)} bytes, "
              f"{len(np.unique(idx))} colors used")


if __name__ == "__main__":
    main()
