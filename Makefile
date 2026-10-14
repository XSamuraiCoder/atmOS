# ----------------------------
# Makefile for the TI-84 CE shell
# ----------------------------

.DEFAULT_GOAL := all

NAME = atmOS
DESCRIPTION = "Calculator shell"
APPLICATION = YES
APPLICATION_DESCRIPTION = "Calculator 
shell"
BINDIR = bin

CFLAGS = -Wall -Wextra -Oz -DSHELL_FLASH_APP=1
CXXFLAGS = -Wall -Wextra -Oz

# To debug memory issues
# CFLAGS = -Wall -Wextra -Oz -DSHELL_FLASH_APP=1 -DSHELL_DEBUG_STACK
# CXXFLAGS = -Wall -Wextra -Oz -DSHELL_DEBUG_STACK

APPVAR_PREFIX = atmOS
APPVAR_SPLIT_SIZE = 65200
CLIBS_VERSION = v15.0
CLIBS_FILE = $(BINDIR)/clibs.8xg

.PHONY: all flash-app appvars installer installer-clean libraries package clean

all: package

flash-app: target

appvars: flash-app
	$(Q)$(CONVBIN) -j 8ek -i $(call QUOTE_ARG,$(BINDIR)/$(NAME).8ek) -k 8xv-split -m $(APPVAR_SPLIT_SIZE) -o $(call QUOTE_ARG,$(BINDIR)/atmOS.8xv) -n $(APPVAR_PREFIX)

libraries: $(CLIBS_FILE)

$(CLIBS_FILE):
	$(Q)$(call MKDIR,$(@D))
	$(Q)curl.exe -fL "https://github.com/CE-Programming/libraries/releases/download/$(CLIBS_VERSION)/clibs.8xg" -o "$(call NATIVEPATH,$@)"

installer:
	$(Q)$(MAKE) -B -C installer APPVAR_PREFIX=$(APPVAR_PREFIX) APPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE) BINDIR=../$(BINDIR)

installer-clean:
	$(Q)$(MAKE) -C installer clean

clean: installer-clean

package: appvars installer libraries

include $(shell cedev-config --makefile)

ifeq ($(OS),Windows_NT)
RMDIR = ( rmdir /s /q $(word 1,$1) 2>nul || call ) & ( rmdir /s /q $(word 2,$1) 2>nul || call )
endif