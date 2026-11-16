# SPDX-License-Identifier: MIT
# Copyright (c) 2026 nulvox
#
# frontend-gbc — gbdk-2020 build of pksave-gbc.gbc.
#
# Invoked inside the pksave-gbc docker container (gbdk-2020 4.3.0).
# The container's CMD is `make -C /src/frontend-gbc all`.
#
# ROM banking layout (MBC5 + 8 ROM banks (128 KiB) + 4 SRAM banks (32 KiB)):
#   Bank 0 (fixed):    session.c, checksum.c, main.c, display.c, input.c,
#                      pksave_bridge.c
#   Bank 1 (switched): gen1.c
#   Bank 2 (switched): gen2.c
#   Bank 3 (switched): names.c, lookups.c
#   Bank 4 (switched): recalc.c, detect.c, limits.c
#   Bank 5 (switched): boot_error_scene.c, main_menu_scene.c, trainer_scene.c,
#                      text_input_scene.c, save_scene.c, bag_scene.c,
#                      slot_edit_scene.c
#   Bank 6 (switched): party_scene.c, box_scene.c, pokemon_edit_scene.c,
#                      pokerus_modal_scene.c

LCC       := lcc
TARGET    := pksave-gbc.gbc
BUILD_DIR := build
PKSAVE    := ../libpksave

CFLAGS_COMMON := \
    -Wf-DPKSAVE_GEN1 \
    -Wf-DPKSAVE_GEN2 \
    -Wf--disable-warning \
    -Wf110 \
    -Isrc \
    -I$(PKSAVE)/include \
    -I$(PKSAVE)/src

LDFLAGS := \
    -Wm-yC \
    -Wm-yn"PKSAVE GBC" \
    -Wl-yt0x1B \
    -Wl-yo8 \
    -Wl-ya4

BANK0_OBJS := \
    $(BUILD_DIR)/session.o \
    $(BUILD_DIR)/checksum.o \
    $(BUILD_DIR)/main.o \
    $(BUILD_DIR)/display.o \
    $(BUILD_DIR)/input.o \
    $(BUILD_DIR)/pksave_bridge.o

BANK1_OBJS := \
    $(BUILD_DIR)/gen1.o

BANK2_OBJS := \
    $(BUILD_DIR)/gen2.o

BANK3_OBJS := \
    $(BUILD_DIR)/names.o \
    $(BUILD_DIR)/lookups.o

BANK4_OBJS := \
    $(BUILD_DIR)/recalc.o \
    $(BUILD_DIR)/detect.o \
    $(BUILD_DIR)/limits.o

BANK5_OBJS := \
    $(BUILD_DIR)/boot_error_scene.o \
    $(BUILD_DIR)/main_menu_scene.o \
    $(BUILD_DIR)/trainer_scene.o \
    $(BUILD_DIR)/text_input_scene.o \
    $(BUILD_DIR)/save_scene.o \
    $(BUILD_DIR)/bag_scene.o \
    $(BUILD_DIR)/slot_edit_scene.o

BANK6_OBJS := \
    $(BUILD_DIR)/party_scene.o \
    $(BUILD_DIR)/box_scene.o \
    $(BUILD_DIR)/pokemon_edit_scene.o \
    $(BUILD_DIR)/pokerus_modal_scene.o

OBJS := \
    $(BANK0_OBJS) \
    $(BANK1_OBJS) \
    $(BANK2_OBJS) \
    $(BANK3_OBJS) \
    $(BANK4_OBJS) \
    $(BANK5_OBJS) \
    $(BANK6_OBJS)

.PHONY: all install clean

all: install

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ── Bank 0 (fixed home bank) ────────────────────────────────────────────────
$(BUILD_DIR)/session.o: $(PKSAVE)/src/session.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) $< -o $@

$(BUILD_DIR)/checksum.o: $(PKSAVE)/src/checksum.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) $< -o $@

$(BUILD_DIR)/main.o: src/main.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) $< -o $@

$(BUILD_DIR)/display.o: src/display.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) $< -o $@

$(BUILD_DIR)/input.o: src/input.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) $< -o $@

$(BUILD_DIR)/pksave_bridge.o: src/pksave_bridge.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) $< -o $@

# ── Bank 1 (Gen I codec) ────────────────────────────────────────────────────
$(BUILD_DIR)/gen1.o: $(PKSAVE)/src/gen1.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo1 $< -o $@

# ── Bank 2 (Gen II codec) ───────────────────────────────────────────────────
$(BUILD_DIR)/gen2.o: $(PKSAVE)/src/gen2.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo2 $< -o $@

# ── Bank 3 (Names & lookups) ────────────────────────────────────────────────
$(BUILD_DIR)/names.o: $(PKSAVE)/src/names.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo3 $< -o $@

$(BUILD_DIR)/lookups.o: $(PKSAVE)/src/lookups.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo3 $< -o $@

# ── Bank 4 (Stat recalc, detect, limits) ────────────────────────────────────
$(BUILD_DIR)/recalc.o: $(PKSAVE)/src/recalc.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo4 $< -o $@

$(BUILD_DIR)/detect.o: $(PKSAVE)/src/detect.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo4 $< -o $@

$(BUILD_DIR)/limits.o: $(PKSAVE)/src/limits.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo4 $< -o $@

# ── Bank 5 (Menu, Trainer, Text Input, Bag, Save scenes) ────────────────────
$(BUILD_DIR)/boot_error_scene.o: src/boot_error_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo5 $< -o $@

$(BUILD_DIR)/main_menu_scene.o: src/main_menu_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo5 $< -o $@

$(BUILD_DIR)/trainer_scene.o: src/trainer_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo5 $< -o $@

$(BUILD_DIR)/text_input_scene.o: src/text_input_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo5 $< -o $@

$(BUILD_DIR)/save_scene.o: src/save_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo5 $< -o $@

$(BUILD_DIR)/bag_scene.o: src/bag_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo5 $< -o $@

$(BUILD_DIR)/slot_edit_scene.o: src/slot_edit_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo5 $< -o $@

# ── Bank 6 (Party, Box, Pokemon Edit, Pokerus Modal scenes) ─────────────────
$(BUILD_DIR)/party_scene.o: src/party_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo6 $< -o $@

$(BUILD_DIR)/box_scene.o: src/box_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo6 $< -o $@

$(BUILD_DIR)/pokemon_edit_scene.o: src/pokemon_edit_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo6 $< -o $@

$(BUILD_DIR)/pokerus_modal_scene.o: src/pokerus_modal_scene.c | $(BUILD_DIR)
	$(LCC) -c $(CFLAGS_COMMON) -Wf-bo6 $< -o $@

# ── Link & install ──────────────────────────────────────────────────────────
$(TARGET): $(OBJS)
	$(LCC) $(LDFLAGS) -o $@ $(OBJS)

install: $(TARGET)
	@mkdir -p ../bin
	@cp -f $(TARGET) ../bin/$(TARGET)
	@ls -l ../bin/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.ihx *.cdb *.noi *.map *.sym
