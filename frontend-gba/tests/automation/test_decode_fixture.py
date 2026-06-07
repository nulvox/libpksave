#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Standalone smoke test for the libpksave ctypes wrapper.

Loads each Gen 3 fixture from libpksave/tests/fixtures/, runs
detect → open → get_trainer_name → close, and asserts the
detected game enum and trainer name match what
libpksave/tests/fixtures/generate_fixtures.py wrote.

Run from anywhere:
    python3 frontend-gba/tests/automation/test_decode_fixture.py

Exits 0 on success, 1 on the first failed assertion.
"""

import os
import sys
import traceback

# Allow running as a plain script (no package context).
_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import pksave_ctypes as pk   # noqa: E402

# Repo root: frontend-gba/tests/automation/ → ../../..
_REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
_FIXTURE_DIR = os.path.join(_REPO_ROOT, "libpksave", "tests", "fixtures")

# Per generate_fixtures.py: all three Gen 3 fixtures encode "RED" as the
# trainer name (gen3_encode("RED", 7) written into section 0 offset 0x00).
EXPECTED_TRAINER_NAME = "RED"

# Gen-3 SRAM is exactly 128 KiB.  The current fixture generator emits files
# slightly larger than that (a known bytearray-slice-grow quirk in
# libpksave/tests/fixtures/generate_fixtures.py — mirrors what test_gen3.c
# already works around by fread-ing only SRAM_SIZE=128 KiB).  We truncate
# to the same size before handing the buffer to libpksave.
GEN3_SRAM_SIZE = 128 * 1024

# (fixture file, expected Game enum, truncate-to-size)
CASES = [
    ("rs.sav",      pk.Game.RS,      GEN3_SRAM_SIZE),
    ("emerald.sav", pk.Game.EMERALD, GEN3_SRAM_SIZE),
    ("frlg.sav",    pk.Game.FRLG,    GEN3_SRAM_SIZE),
]


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    sys.exit(1)


def assert_eq(actual, expected, label: str) -> None:
    if actual != expected:
        fail(f"{label}: expected {expected!r}, got {actual!r}")


def run_case(lib, filename: str, expected_game: pk.Game,
             sram_size: int) -> None:
    path = os.path.join(_FIXTURE_DIR, filename)
    if not os.path.isfile(path):
        fail(f"fixture missing: {path}")
    with builtins_open(path, "rb") as f:
        raw = f.read()
    if len(raw) < sram_size:
        fail(f"{filename}: fixture is {len(raw)} bytes, need at least "
             f"{sram_size}")
    buf = raw[:sram_size]

    # detect()
    detected = pk.detect(buf, lib=lib)
    assert_eq(detected, expected_game, f"{filename} detect()")

    # open() + get_trainer_name()
    save = pk.open(buf, expected_game, lib=lib)
    try:
        name = pk.get_trainer_name(save)
        assert_eq(name, EXPECTED_TRAINER_NAME,
                  f"{filename} trainer name")
        print(f"  OK  {filename:12s} detect={detected.name:7s} "
              f"name={name!r}")
    finally:
        pk.close(save)


# Avoid shadowing built-in open() inside this script.
import builtins
builtins_open = builtins.open


def main() -> int:
    try:
        lib = pk.load_library()
    except Exception:
        traceback.print_exc()
        return 1

    print(f"libpksave loaded: {lib._name}")
    print(f"fixtures dir:     {_FIXTURE_DIR}")
    print("running 3 Gen-3 decode cases:")

    for fname, game, sram_size in CASES:
        try:
            run_case(lib, fname, game, sram_size)
        except SystemExit:
            raise
        except Exception:
            print(f"FAIL: unexpected exception decoding {fname}", file=sys.stderr)
            traceback.print_exc()
            return 1

    print("PASS: 3/3 fixtures decoded successfully")
    return 0


if __name__ == "__main__":
    sys.exit(main())
