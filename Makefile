# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# Root Makefile — all compilation is delegated to Docker.
# Only Docker is required on the host; no cross-compilers needed.
#
# Targets:
#   make test   — run libpksave Unity tests (docker/test)
#   make gbc    — build pksave-gbc.gbc     (docker/gbc)
#   make gba    — build pksave-gba.gba     (docker/gba)
#   make nds    — build pksave-nds.nds     (docker/nds)
#   make all    — test + gbc + gba + nds
#   make clean  — remove bin/ artifacts

DOCKER    := docker
REPO_ROOT := $(CURDIR)
BIN_DIR   := $(REPO_ROOT)/bin

.PHONY: test gbc gba nds all clean \
        _build_test _build_gbc _build_gba _build_nds

# ── test ─────────────────────────────────────────────────────────────────────
test: _build_test
	$(DOCKER) run --rm \
		-v "$(REPO_ROOT):/src" \
		pksave-test

_build_test:
	$(DOCKER) build -t pksave-test docker/test

# ── gbc ──────────────────────────────────────────────────────────────────────
gbc: _build_gbc
	$(DOCKER) run --rm \
		-v "$(REPO_ROOT):/src" \
		pksave-gbc

_build_gbc:
	$(DOCKER) build -t pksave-gbc docker/gbc

# ── gba ──────────────────────────────────────────────────────────────────────
gba: _build_gba
	$(DOCKER) run --rm \
		-v "$(REPO_ROOT):/src" \
		pksave-gba

_build_gba:
	$(DOCKER) build -t pksave-gba docker/gba

# ── nds ──────────────────────────────────────────────────────────────────────
nds: _build_nds
	$(DOCKER) run --rm \
		-v "$(REPO_ROOT):/src" \
		pksave-nds

_build_nds:
	$(DOCKER) build -t pksave-nds docker/nds

# ── all ───────────────────────────────────────────────────────────────────────
all: test gbc gba nds

# ── clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -f $(BIN_DIR)/*.gbc $(BIN_DIR)/*.gba $(BIN_DIR)/*.nds
	rm -f libpksave/tests/test_detect \
	      libpksave/tests/test_gen1 \
	      libpksave/tests/test_gen2 \
	      libpksave/tests/test_gen3 \
	      libpksave/tests/test_gen4 \
	      libpksave/tests/test_gen5 \
	      libpksave/tests/test_limits
