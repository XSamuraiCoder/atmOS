# ----------------------------
# Makefile for the TI-84 CE shell (atmOS)
# ----------------------------

# NEW: src/asm/osLaunch.s (OS-side launch hook) needs no entry here. The
# toolchain makefile included below compiles every .s file it finds under
# src/ recursively, so simply keeping the file in src/asm/ adds it to this
# Flash App build automatically. It externs _flash_program_return_to_shell,
# so never build it into a non-Flash-App target that shares this src/ tree
# (a separate program build would have to exclude it).

.DEFAULT_GOAL := all

NAME = atmOS
DESCRIPTION = "Calculator shell"
APPLICATION = YES
APPLICATION_DESCRIPTION = "Calculator shell"
BINDIR = bin

CFLAGS = -Wall -Wextra -Oz -DSHELL_FLASH_APP=1
CXXFLAGS = -Wall -Wextra -Oz -DSHELL_FLASH_APP=1

# make TRACE=1 builds the boot-trace version (stops after each startup stage, prints addresses)
ifeq ($(TRACE),1)
CFLAGS   += -DSHELL_BOOT_TRACE
CXXFLAGS += -DSHELL_BOOT_TRACE
endif

# make DEBUG_STACK=1 builds the stack-debug version (draws the SP low-water mark in
# the top bar; "OSSTK" additionally means the shell fell back to the OS stack)
ifeq ($(DEBUG_STACK),1)
CFLAGS   += -DSHELL_DEBUG_STACK
CXXFLAGS += -DSHELL_DEBUG_STACK
endif

APPVAR_PREFIX = ATMOS
APPVAR_SPLIT_SIZE = 65200
CLIBS_VERSION = v15.0
CLIBS_FILE = $(BINDIR)/clibs.8xg

.PHONY: all flash-app appvars installer installer-clean libraries package clean

all: package

flash-app: target

appvars: flash-app
	$(Q)$(CONVBIN) -j 8ek -i $(call QUOTE_ARG,$(BINDIR)/$(NAME).8ek) -k 8xv-split -m $(APPVAR_SPLIT_SIZE) -o $(call QUOTE_ARG,$(BINDIR)/atmOS.8xv) -n $(APPVAR_PREFIX)

libraries: $(CLIBS_FILE)

# Tries the pinned release first and falls back to the latest one if that tag does not exist.
$(CLIBS_FILE):
	$(Q)$(call MKDIR,$(@D))
	$(Q)curl -fL "https://github.com/CE-Programming/libraries/releases/download/$(CLIBS_VERSION)/clibs.8xg" -o "$(call NATIVEPATH,$@)" || curl -fL "https://github.com/CE-Programming/libraries/releases/latest/download/clibs.8xg" -o "$(call NATIVEPATH,$@)"

installer:
	$(Q)$(MAKE) -B -C installer APPVAR_PREFIX=$(APPVAR_PREFIX) APPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE) BINDIR=../$(BINDIR)

installer-clean:
	$(Q)$(MAKE) -C installer clean

package: appvars installer libraries

include $(shell cedev-config --makefile)

ifeq ($(OS),Windows_NT)
RMDIR = ( rmdir /s /q $(word 1,$1) 2>nul || call ) & ( rmdir /s /q $(word 2,$1) 2>nul || call )
endif