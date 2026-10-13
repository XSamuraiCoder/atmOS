# Flash App build and installation

Run `make` with the CE C/C++ Toolchain installed. All generated deliverables
are placed in the shared `bin/` folder:

- `bin/SHELL.8ek`: the Flash App payload.
- `bin/SHELL.0.8xv` (and any subsequent numbered files): AppVars named
  `APP0`, `APP1`, etc., containing the split payload.
- `bin/INSTALL.8xp`: the separate installer program.
- `bin/clibs.8xg`: the CE runtime libraries required by the shell at startup.

Build steps are available individually with `make flash-app`, `make appvars`,
`make installer`, and `make libraries`. `make package` (also the default
`make` target) builds the AppVars, installer, and required runtime-library
group; `make -j package` may build independent steps in parallel. The
`clibs.8xg` download is pinned to CE-Programming/libraries v15.0.
The installer uses the same `BSSHEAP_LOW` as the app, so data-address
relocations match the app's linked RAM address. Always use the AppVars and
installer from the same build.

Transfer `clibs.8xg` to the calculator with TI Connect CE and install/archive
the group before running the shell. The production shell requires `LibLoad`,
`FILEIOC`, `GRAPHX`, and `KEYPADC`. Without a required library, the application
cannot start and may return to the calculator home screen.

Then transfer `INSTALL.8XP` and **all** `SHELL.*.8xv` files from `bin/` to the
calculator. Run `INSTALL` from the calculator's program menu and follow its
prompts. The installer offers to delete itself and the payload AppVars after a
successful install; the installed app remains in the APPS menu. Keep the CE
library AppVars archived; they are required every time the app starts.

Run `make clean` before rebuilding if you change the AppVar prefix or split
size, or if an older build left extra numbered payload files.

## Files test builds

The production shell now uses the FILEIOC-backed Files browser. It caches
decoded embedded TI program icons and DCS program icons for the visible page
only, keeping icon-cache RAM bounded. Both test builds use this shared desktop
UI and Files browser. The no-FILEIOC variant uses CE OS
symbol-table/variable-size APIs, while the FILEIOC variant uses FILEIOC.

Build each test and its matching installer:

```text
make -C test-files-fileioc
make -C installer -B APPVAR_PREFIX=FI BINDIR=../test-files-fileioc-installer-bin
make -C test-files-no-fileioc
make -C installer -B APPVAR_PREFIX=NF BINDIR=../test-files-no-fileioc-installer-bin
```

For the FILEIOC test, transfer
`test-files-fileioc-bin/FILETEST.0.8xv` (`FI0`) and
`test-files-fileioc-installer-bin/INSTALL.8xp`, run `INSTALL`, then launch
`FILETEST`. It requires FILEIOC, GRAPHX, and KEYPADC.

For the no-FILEIOC test, transfer `test-files-no-fileioc-bin/NOFILE.0.8xv`
(`NF0`) and `test-files-no-fileioc-installer-bin/INSTALL.8xp`, run `INSTALL`,
then launch `NOFILE`. It requires only GRAPHX and KEYPADC. Remove the
corresponding installed test app before reinstalling a rebuilt version.

The no-FILEIOC build uses `os_GetSymTablePtr`, `os_NextSymEntry`, and
`os_GetVarSize`; it does not open or read variable contents. The test builds
omit icon caching. The FILEIOC build was confirmed to launch and show
variable sizes on the calculator. The Apps folder scans installed Flash App
headers and exposes each app as a launchable tile.

Build the production shell with `make appvars installer`. It is emitted as
`bin/SHELL.8ek` and `bin/SHELL.0.8xv` (AppVar `APP0`); the matching installer
is `bin/INSTALL.8xp`.

This installer writes to Flash. Back up calculator data first, use only a
compatible TI-84 Plus CE and OS, and do not reset or power off during the
installation. The installer performs OS, flash-space, and overlap checks, but a
power loss or device-specific incompatibility can still leave the calculator
in an unusable state. This build does not install or invoke arTIfiCE.

In Files, select a BASIC, ASM, C, ICE program, or installed Flash App and press
Alpha to open its taskbar menu. Choose **Pin to taskbar** or **Unpin from
taskbar** with the arrow keys and Enter; Clear dismisses the menu. Up to six
items can be pinned alongside the three built-in dock icons. Pins persist in
the archived `SHLPINS` AppVar; keep it on the calculator to retain them.
Embedded program and Flash App icons retain their source colors using the
TI icon palette mapping and the shell's expanded dock palette; palette index
`0xFF` is rendered as white. Items without icons use a letter fallback.
See [THIRD_PARTY_NOTICES.txt](./THIRD_PARTY_NOTICES.txt) for palette attribution.

The Flash App build launches BASIC, ASM, C, and ICE programs through a
CEaShell-derived TI OS runner. It switches from the Flash App's C stack to the
TI-OS stack before preparing program memory. On normal program exit, SHELL is
relaunched;
the TI-OS monitor-abandon path can return to the calculator OS.
The Apps folder enumerates installed Flash Apps and hands execution to the
selected app. Flash App launching uses undocumented, OS-version-dependent TI
OS routines; quitting a launched app returns to the calculator home screen
rather than resuming SHELL. Test both launch paths carefully on the calculator
before relying on them.

The program runner incorporates GPL-3.0 code from
[CEaShell](https://github.com/RoccoLoxPrograms/CEaShell) by RoccoLox Programs
and TIny_Hacker. This project is distributed under the GNU General Public
License, version 3; see [LICENSE](./LICENSE).
