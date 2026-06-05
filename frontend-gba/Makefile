# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# frontend-gba — butano build of pksave-gba.gba.
#
# Invoked inside the pksave-gba docker container (devkitARM + butano 18.5.0).
# The container's CMD is `make -C /src/frontend-gba all`.
#
# Build strategy: libpksave's C sources are compiled directly into the
# butano build (option (a) in the plan), gated on -DPKSAVE_GEN3.  No
# separate static-library / ar step — butano's existing link rules
# handle ordering.
#
# Final artifact: /src/bin/pksave-gba.gba (copied from the in-tree
# build by the `install` target at the outer level).

#-------------------------------------------------------------------------------
# Project metadata — names match the butano template Makefile exactly so
# butano.mak's `include` pattern can re-read this Makefile cleanly in
# both the outer and inner (build/) passes.
#-------------------------------------------------------------------------------
TARGET       :=  pksave-gba
BUILD        :=  build
LIBBUTANO    :=  $(BUTANO_HOME)/butano
PYTHON       :=  python3
SOURCES      :=  src src/scenes src/widgets ../libpksave/src
INCLUDES     :=  src src/widgets ../libpksave/include ../libpksave/src
DATA         :=
# Pull only the bitmap fonts out of butano's common/.  Including
# common/src would drag in demo helpers (info / stats screens) we
# don't use.  grit converts each .bmp into a bn_sprite_items_*.h
# header under build/.
#
# We can't put $(BUTANO_HOME)/common/graphics in GRAPHICS because
# butano's GRAPHICS list is also prefixed with $(CURDIR), which
# garbles absolute paths.  Instead, the graphics live under our own
# graphics/ via a symlink-style copy at build time (handled below).
GRAPHICS     :=  graphics
AUDIO        :=  audio
DMGAUDIO     :=  dmg_audio
ROMTITLE     :=  PKSAVE GBA
ROMCODE      :=  PKSE

# libpksave is gated on -DPKSAVE_GEN3 per the plan.
# FLASH 128KB requires wait-state 3 on the cartridge — slower than the
# butano SRAM default of 8.  Detection of FLASH (vs SRAM) by emulators
# is via the magic string "FLASH1M_V" embedded by save_type_signature.cpp.
#
# Butano's `INCLUDES` list is prefixed with $(CURDIR), so absolute paths
# (e.g. $(BUTANO_HOME)/common/include) get mangled.  We feed those via
# USERFLAGS instead, which is appended verbatim to CFLAGS / CXXFLAGS.
USERFLAGS    :=  -DPKSAVE_GEN3 \
                 -DBN_CFG_SRAM_WAIT_STATE=BN_SRAM_WAIT_STATE_3 \
                 -I$(BUTANO_HOME)/common/include \
                 -std=gnu17
USERCXXFLAGS :=
USERASFLAGS  :=
USERLDFLAGS  :=
USERLIBDIRS  :=
USERLIBS     :=
DEFAULTLIBS  :=
STACKTRACE   :=
USERBUILD    :=
# EXTTOOL runs once before the asset pipeline / build.  We use it to
# stage butano's bundled common-font BMPs into our local graphics/ so
# grit picks them up — the GRAPHICS list also gets $(CURDIR)-prefixed
# and so can't take absolute paths.
EXTTOOL      :=  cp -u $(BUTANO_HOME)/common/graphics/common_*.bmp $(BUTANO_HOME)/common/graphics/common_*.json graphics/

#-------------------------------------------------------------------------------
# Export the absolute butano path the way butano's template does.
#-------------------------------------------------------------------------------
ifndef LIBBUTANOABS
    export LIBBUTANOABS := $(realpath $(LIBBUTANO))
endif

#-------------------------------------------------------------------------------
# Include butano's makefile.  This sets up the `all` and `clean`
# targets, plus the asset pipeline and link rules.  The same Makefile
# is re-read by butano's inner sub-make from $(BUILD)/, so anything we
# add below must be safe in both contexts.
#-------------------------------------------------------------------------------
include $(LIBBUTANOABS)/butano.mak

#-------------------------------------------------------------------------------
# Outer-pass-only: add an `install` target that copies $(TARGET).gba
# into /src/bin/.  The inner build (CURDIR=$(BUILD)) doesn't have the
# .gba in CURDIR, and we don't want install rules redefined there.
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

.PHONY: install
install: all
	@mkdir -p ../bin
	@cp -f $(TARGET).gba ../bin/$(TARGET).gba
	@ls -l ../bin/$(TARGET).gba

# Make `all` (butano's default) also perform the copy.  We can't redefine
# `all` directly without confusing butano, so the docker CMD invokes
# `install` explicitly via this wrapper:
.PHONY: gba
gba: install

endif
