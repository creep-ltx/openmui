# The release version: one number, in the file VERSION at the top of the
# tree. Every place that shows it (the library's, the prefs program's and
# OpenMUISwitch's $VER, the About window, the installer, the readmes and
# the archive name) gets it from there at build or release time.
# ZUNE68_DATE is the day of the build (DD.MM.YYYY) unless given to make.
OPENMUI_VERSION := $(strip $(shell cat $(dir $(lastword $(MAKEFILE_LIST)))../VERSION))
ZUNE68_DATE ?= $(shell date +%d.%m.%Y)
OPENMUI_VERSION_FLAGS = -DZUNE68_EDITION_NUMBER='"$(OPENMUI_VERSION)"' \
  -DOPENMUI_BUILD_DATE='"$(ZUNE68_DATE)"'
