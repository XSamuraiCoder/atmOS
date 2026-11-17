# Leaves: animated wallpaper mod for atmOS

`make` builds `bin/LEAVES.8xv` (archived). Send it to the calculator and restart atmOS: it adds

- **Leaves** in Settings > Look > Wallpaper: an autumn dusk (sky, low sun, hills) with 12
  leaves drifting down, swaying and tumbling.
- **Autumn** in Settings > Look > Theme, paired with it: picking the theme picks the wallpaper,
  which can still be changed afterwards.

Needs a shell with mod API 1.2 (animated wallpapers). Everything is drawn from code in
`src/leaves.c`; there is no picture data.

## Build

    # cedev-config (CEdev's bin folder) on PATH, and Python 3 for tools/mkmod.py
    make
    make clean

`src/atmos_mod.h` is a copy of the shell's `src/atmos_mod.h`: copy it again when that changes.
