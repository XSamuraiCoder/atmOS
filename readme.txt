atmOS - a pointer-driven shell for the TI-84 Plus CE
=====================================================

atmOS gives your calculator a desktop-style home screen: a menu bar on
top, a dock at the bottom, and windows for Files, Notes and Settings.
You move a pointer around with the arrow keys and click with Enter.

It runs as a Flash App, so it lives in the Apps menu.


WHAT YOU NEED
-------------
  - TI-84 Plus CE or TI-83 Premium CE (or the CEmu emulator)
  - A way to send files: TI Connect CE, TILP, or just drag the files
    into CEmu
  - A jailbreak like arTIfiCE to run the installer
  - Enough free archive memory for the app


THE FILES
---------
Download atmOS.zip and unzip it. Inside you will find:

  clibs.8xg       The CE C libraries that atmOS needs.
  atmOS...8xv     The app itself. There may be more than one of these.
  INSTALL.8xp     The installer program.


HOW TO INSTALL
--------------
 1. Send clibs.8xg to the calculator.
    (Skip this if you already have the CE C libraries installed.)

 2. Send ALL the atmOS .8xv files.
    Send every one of them, or the install will fail.

 3. Send INSTALL.8xp.

 4. On the calculator, run your jailbreak, choose INSTALL from the menu or the [prgm] menu, and press [enter].
    (On an older OS, if it shows an error, run it as Asm(prgmINSTALL)
    instead. Asm( is in [2nd] [catalog].)

 5. Wait for the installer to finish. Do not remove the batteries.

 6. Optional: once it is done, delete INSTALL and the atmOS .8xv files
    to free memory. The app is already installed.

To start atmOS, press [apps] and choose atmOS.

To quit atmOS, press [clear] while no window is open.


HOW TO USE IT
-------------
Keys:

  Arrow keys      Move the pointer.
  [enter]/[2nd]   Click.
  [clear]         Go back / close the window / close a menu.
                  With nothing open, it exits atmOS.
  [alpha]         In Files: open the pin menu for the item under the
                  pointer.

Screen layout:

  Top bar         Status text on the left. Battery and the time on the
                  right.
  Dock            The icons at the bottom. Click one to open it.
                  With "Dock labels" on in Settings, hovering over an
                  icon shows its name.
  Windows         Click the dot at the top-left of a window to close it.

Built-in apps:

  Files
    Browse your Programs, AppVars and Apps as big tiles.
    Click a tile to select it. Click a program again to run it.
    Move the pointer off the left or right edge to flip pages.
    Pin a program or app to the dock: point at it, press [alpha],
    choose "Pin to taskbar". Use the same menu to unpin it.
    The dock holds up to 6 pinned items.

  Notes
    Click "+ New note" to start one, or click an existing note.
    Typing:  [alpha] switches between numbers and letters.
             [2nd] switches between UPPER and lower case.
             [del] erases.
    On the notes list, point at a note and press [del] twice to
    delete it.
    [clear] goes back to the list. Notes are saved when you leave atmOS.
    NOTE: having a NOTES prgm make the notes app run that instead.
  Settings
    Three tabs: Look, Taskbar, System.
      Look      Theme, wallpaper, accent color, font.
      Taskbar   Menu bar color, dock color, battery indicator,
                dock labels.
      System    Pointer speed, 12/24-hour clock, reset all settings.
    Click a dropdown to open it, then click a choice. While a
    dropdown is open the pointer stays inside it. [clear] closes it.
    Changes apply right away and are saved when you leave atmOS.

UPDATING
--------
Send the new files and run INSTALL again. Your settings, notes and
pinned items are kept.


UNINSTALLING
------------
Press [2nd] [+] (Mem), choose Mem Mgmt/Del, then Apps, select atmOS
and press [del].

Your settings and notes are stored in AppVars. Delete these too if
you want everything gone:  SHLSET (settings).


IF SOMETHING GOES WRONG
-----------------------
  - Installer error: check that you sent ALL the .8xv files and
    clibs.8xg, then run INSTALL again.
  - Not enough memory: archive or delete things you do not need and
    try again.
  - Strange look after changing the font: open Settings and pick
    Default.
  - Settings acting odd: Settings > System > Reset all.
  - Calculator resets: note what you were doing just before it
    happened, and report it.