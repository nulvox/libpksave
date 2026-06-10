# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# Root Makefile — all compilation is delegated to Docker.
# Only Docker is required on the host; no cross-compilers needed.
#
# Targets:
#   make test     — run libpksave Unity tests (docker/test)
#   make test-gba — run frontend-gba mGBA-Lua harness tests (docker/test-gba)
#   make gbc      — build pksave-gbc.gbc     (docker/gbc)
#   make gba      — build pksave-gba.gba     (docker/gba)
#   make nds      — build pksave-nds.nds     (docker/nds)
#   make all      — test + gbc + gba + nds
#   make clean    — remove bin/ artifacts

DOCKER    := docker
PYTHON    := python3
REPO_ROOT := $(CURDIR)
BIN_DIR   := $(REPO_ROOT)/bin

GBA_TESTS := \
    frontend-gba/tests/automation/test_decode_fixture.py \
    frontend-gba/tests/automation/test_mgba_harness_smoke.py \
    frontend-gba/tests/automation/test_editor_rom_loads.py \
    frontend-gba/tests/automation/test_sram_dump.py \
    frontend-gba/tests/automation/test_sram_dump_full.py \
    frontend-gba/tests/automation/test_flash_detected.py \
    frontend-gba/tests/automation/test_key_input.py \
    frontend-gba/tests/automation/test_sidecar_save.py \
    frontend-gba/tests/automation/test_fixture_roundtrip.py \
    frontend-gba/tests/automation/test_trainer_scene_nav.py \
    frontend-gba/tests/automation/test_trainer_name_roundtrip.py \
    frontend-gba/tests/automation/test_boot_rejects_bad_save.py \
    frontend-gba/tests/automation/test_tid_roundtrip.py \
    frontend-gba/tests/automation/test_main_menu_wrap.py \
    frontend-gba/tests/automation/test_cycler_host.py \
    frontend-gba/tests/automation/test_party_scene_nav.py \
    frontend-gba/tests/automation/test_party_species_roundtrip.py \
    frontend-gba/tests/automation/test_pokemon_edit_tab_nav.py \
    frontend-gba/tests/automation/test_party_nickname_roundtrip.py \
    frontend-gba/tests/automation/test_pokerus_modal_cancel.py \
    frontend-gba/tests/automation/test_pokerus_modal_commit.py \
    frontend-gba/tests/automation/test_stats_level_roundtrip.py \
    frontend-gba/tests/automation/test_stats_ev_roundtrip.py \
    frontend-gba/tests/automation/test_moves_move_roundtrip.py \
    frontend-gba/tests/automation/test_moves_pp_roundtrip.py \
    frontend-gba/tests/automation/test_bag_scene_nav.py \
    frontend-gba/tests/automation/test_bag_slot_edit_roundtrip.py \
    frontend-gba/tests/automation/test_box_scene_nav.py \
    frontend-gba/tests/automation/test_box_species_roundtrip.py \
    frontend-gba/tests/automation/test_box_stats_level_roundtrip.py

.PHONY: test test-gba test-tools gbc gba nds all clean \
        _build_test _build_test_gba _build_gbc _build_gba _build_nds \
        _maybe_extract_sprites _gen_sprite_lookup

# ── test ─────────────────────────────────────────────────────────────────────
test: _build_test
	$(DOCKER) run --rm \
		-v "$(REPO_ROOT):/src" \
		pksave-test

_build_test:
	$(DOCKER) build -t pksave-test docker/test

# ── test-gba ─────────────────────────────────────────────────────────────────
# Runs the mGBA-Lua automation tests for the frontend-gba editor.
# Two prereqs: libpksave.so (for ctypes decode) and the pksave-test-gba image
# (for the headless mGBA harness). Tests themselves run on the host and
# subprocess into docker for the emulator runs.
test-gba: _build_test_gba libpksave/libpksave.so
	@failed=0; \
	for t in $(GBA_TESTS); do \
		printf -- '--- %s ---\n' "$$t"; \
		$(PYTHON) "$$t" || failed=1; \
	done; \
	[ $$failed -ne 0 ] && { echo "=== test-gba FAILED ==="; exit 1; }; \
	echo "=== test-gba PASSED ==="

_build_test_gba:
	$(DOCKER) build -t pksave-test-gba docker/test-gba

libpksave/libpksave.so:
	$(MAKE) -C libpksave lib

# ── gbc ──────────────────────────────────────────────────────────────────────
gbc: _build_gbc
	$(DOCKER) run --rm \
		-v "$(REPO_ROOT):/src" \
		pksave-gbc

_build_gbc:
	$(DOCKER) build -t pksave-gbc docker/gbc

# ── gba ──────────────────────────────────────────────────────────────────────
# Optional: SPRITES_ROM=/path/to/gen3.gba runs the sprite extractor before
# the butano build so the editor ships with real Pokemon sprites instead
# of the default silhouette. The repo never bundles extracted sprite art
# (frontend-gba/assets/sprites/ is .gitignored).
gba: _build_gba _maybe_extract_sprites _gen_sprite_lookup
	$(DOCKER) run --rm \
		-v "$(REPO_ROOT):/src" \
		pksave-gba

_build_gba:
	$(DOCKER) build -t pksave-gba docker/gba

_maybe_extract_sprites:
ifneq ($(strip $(SPRITES_ROM)),)
	$(PYTHON) tools/extract_sprites.py \
		--rom "$(SPRITES_ROM)" \
		--out frontend-gba/graphics/sprites/
endif

# Regenerate species_sprite_lookup.cpp on every gba build so it stays
# in sync with whatever BMPs landed in frontend-gba/graphics/sprites/.
# The generated file is gitignored: fresh clones materialize an empty
# stub here (no `make gba SPRITES_ROM=...` ever run) and get the
# silhouette-only behavior. `SPRITES_ROM=...` runs above this step
# and replaces the BMPs, then this step regenerates the lookup.
_gen_sprite_lookup:
	$(PYTHON) tools/gen_sprite_lookup.py \
		--sprites-dir frontend-gba/graphics/sprites/ \
		--out frontend-gba/src/species_sprite_lookup.cpp

# ── test-tools ───────────────────────────────────────────────────────────────
# Pure-Python unit tests for tools/ — no docker, no devkitARM.
test-tools:
	$(PYTHON) tools/test_extract_sprites.py
	$(PYTHON) tools/test_gen_sprite_lookup.py

# ── nds ──────────────────────────────────────────────────────────────────────
nds: _build_nds
	$(DOCKER) run --rm \
		-v "$(REPO_ROOT):/src" \
		pksave-nds

_build_nds:
	$(DOCKER) build -t pksave-nds docker/nds

# ── all ───────────────────────────────────────────────────────────────────────
all: test test-tools gbc gba nds

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -f $(BIN_DIR)/*.gbc $(BIN_DIR)/*.gba $(BIN_DIR)/*.nds
	rm -f libpksave/tests/test_detect \
	      libpksave/tests/test_gen1 \
	      libpksave/tests/test_gen2 \
	      libpksave/tests/test_gen3 \
	      libpksave/tests/test_gen3_charset \
	      libpksave/tests/test_gen3_lookups \
	      libpksave/tests/test_gen3_recalc \
	      libpksave/tests/test_fixtures \
	      libpksave/tests/test_gen4 \
	      libpksave/tests/test_gen5 \
	      libpksave/tests/test_limits
