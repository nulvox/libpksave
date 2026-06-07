# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
#
# Build libpksave tests.
# Invoked by the docker/test container as:
#   make -C /src/libpksave test UNITY_DIR=/opt/unity/src

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic \
          -DPKSAVE_GEN1 -DPKSAVE_GEN2 -DPKSAVE_GEN3 -DPKSAVE_GEN4 -DPKSAVE_GEN5
# The Gen 3 charset round-trip test shares its character list with the
# frontend-gba text_input widget — include path points at the widgets
# directory so test_gen3_charset.c can #include "text_input_charset.h".
INCLUDE = -Iinclude -Isrc -I$(UNITY_DIR) -I../frontend-gba/src/widgets

SRC = src/detect.c \
      src/checksum.c \
      src/session.c \
      src/names.c \
      src/limits.c \
      src/lookups.c \
      src/recalc.c \
      src/gen1.c \
      src/gen2.c \
      src/gen3.c \
      src/gen4.c \
      src/gen5.c

UNITY_SRC = $(UNITY_DIR)/unity.c

TEST_BINS = \
    tests/test_detect \
    tests/test_gen1 \
    tests/test_gen2 \
    tests/test_gen3 \
    tests/test_gen3_charset \
    tests/test_gen3_lookups \
    tests/test_gen3_recalc \
    tests/test_gen4 \
    tests/test_gen5 \
    tests/test_limits \
    tests/test_fixtures

.PHONY: test fixtures clean lib

fixtures:
	python3 tests/fixtures/generate_fixtures.py tests/fixtures

test: fixtures $(TEST_BINS)
	@echo "=== running libpksave unit tests ==="
	@failed=0; \
	for t in $(TEST_BINS); do \
		printf -- '--- %s ---\n' "$$t"; \
		./$$t; \
		[ $$? -ne 0 ] && failed=1; \
	done; \
	[ $$failed -ne 0 ] && { echo "=== TESTS FAILED ==="; exit 1; }; \
	echo "=== ALL TESTS PASSED ==="

$(TEST_BINS): %: %.c $(SRC) $(UNITY_SRC)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $^

# ── Shared-library build for the Python ctypes wrapper ─────────────────────
# Built with -fPIC -shared and only the libpksave headers/sources (no Unity).
# The frontend-gba widget include path is also added for parity with the test
# build, even though the SO doesn't actually pull anything from there.
LIB_CFLAGS = -std=c99 -Wall -Wextra -pedantic -fPIC -shared \
             -DPKSAVE_GEN1 -DPKSAVE_GEN2 -DPKSAVE_GEN3 -DPKSAVE_GEN4 -DPKSAVE_GEN5
LIB_INCLUDE = -Iinclude -Isrc

libpksave.so: $(SRC)
	$(CC) $(LIB_CFLAGS) $(LIB_INCLUDE) -o $@ $^

lib: libpksave.so

clean:
	rm -f $(TEST_BINS) libpksave.so
