# ----------------------------------------------------
# atmOS installer - CEdev project (program + assembly)
# ----------------------------------------------------

NAME ?= ATMOS
DESCRIPTION ?= "atmOS Flash App installer"
ARCHIVED ?= NO

APPVAR_SPLIT_SIZE ?= 0x7F00

CFLAGS   ?= -Wall -Wextra -O2
CXXFLAGS ?= -Wall -Wextra -O2

CFLAGS   += -DAPPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)
CXXFLAGS += -DAPPVAR_SPLIT_SIZE=$(APPVAR_SPLIT_SIZE)

# ----------------------------------------------------
# Load CEdev build rules
include $(shell cedev-config --makefile)