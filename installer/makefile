# ----------------------------------------------------
# atmOS installer - CEdev project (program + assembly)
# ----------------------------------------------------
# Installs the atmOS Flash App on the calculator from split AppVars
# ("ATMOS0", "ATMOS1", ...): erases the flash pages behind the last app,
# writes the image, appends its size, then applies the app's relocation
# table. See README.md and src/config.h for the packaging contract.

NAME ?= ATMOS
DESCRIPTION ?= "atmOS Flash App installer"
ARCHIVED ?= NO

CFLAGS   ?= -Wall -Wextra -O2
CXXFLAGS ?= -Wall -Wextra -O2

# ----------------------------------------------------

# same way the atmOS shell's Makefile finds CEdev (cedev-config must be on PATH);
# makefile.mk assembles src/*.asm (ez80-asm) and links it with the program
include $(shell cedev-config --makefile)