# Installer based on CE-Programming/app_tools; see UPSTREAM.txt.

NAME = INSTALL
COMPRESSED = NO
ARCHIVED = NO
LTO = NO
HAS_PRINTF = NO

APPVAR_PREFIX = atmOS
APPVAR_SPLIT_SIZE = 65200
BINDIR = ../bin

# Keep data-address relocations aligned with the app's linker-assigned RAM base.
APP_DATA_BASE ?= $(BSSHEAP_LOW)

ASFLAGS = --defsym=APP_DATA_BASE=$(APP_DATA_BASE)

CFLAGS = -Wall -Wextra -Oz -DAPPVAR_PREFIX=\"$(APPVAR_PREFIX)\" -DAPPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)
CXXFLAGS = -Wall -Wextra -Oz -DAPPVAR_PREFIX=\"$(APPVAR_PREFIX)\" -DAPPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)

include $(shell cedev-config --makefile)

ifeq ($(OS),Windows_NT)
RMDIR = ( rmdir /s /q $(word 1,$1) 2>nul || call ) & ( rmdir /s /q $(word 2,$1) 2>nul || call )
endif