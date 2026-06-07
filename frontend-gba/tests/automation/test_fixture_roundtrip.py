#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Gate H — full fixture round-trip through the harness.

Combines: Track C (`--save <path>` sidecar load), Track E (full-region
SRAM dump pattern), and the libpksave ctypes wrapper. This is the
smallest convergence test that proves the harness can produce a
libpksave-parseable save buffer end-to-end.

Flow:
    1. Spawn docker with --save pointing at rs.sav (RW mount required
       because mCoreLoadSaveFile opens with O_CREAT|O_RDWR).
    2. Lua runs 60+ frames so editor's flash-unlock command fires before
       the shutdown-callback dump.
    3. Python reads the dump, hands it to pksave_ctypes, asserts:
       - detect == RS
       - trainer name == 'RED' (the canonical fixture name from
         generate_fixtures.py).

Run from the repo root:
    python3 frontend-gba/tests/automation/test_fixture_roundtrip.py
"""

import os
import shutil
import subprocess
import sys
import tempfile

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)
import pksave_ctypes as pk  # noqa: E402

DOCKER_IMAGE = "pksave-test-gba"
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
DUMP_LUA_CONTAINER = "/src/frontend-gba/tests/automation/dump_after_boot.lua"
FRAMES = "120"
TIMEOUT_SECONDS = 120

# All three Gen-3 fixtures encode the same canonical trainer name "RED" per
# libpksave/tests/fixtures/generate_fixtures.py.  Per-variant testing catches
# variant-specific section-layout / checksum bugs that the single-fixture
# tracer can't.
EXPECTED_TRAINER_NAME = "RED"
CASES = [
    ("rs.sav",      pk.Game.RS),
    ("emerald.sav", pk.Game.EMERALD),
    ("frlg.sav",    pk.Game.FRLG),
]


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout ---\n{result.stdout}", file=sys.stderr)
        print(f"--- stderr ---\n{result.stderr}", file=sys.stderr)
    sys.exit(1)


def run_case(fixture_name, expected_game, lib):
    fixture_src = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", fixture_name)
    if not os.path.isfile(fixture_src):
        fail(f"fixture missing: {fixture_src}")

    with tempfile.TemporaryDirectory(prefix=f"pksave-rt-{fixture_name}-") as workdir:
        dump_dir = os.path.join(workdir, "dump")
        save_dir = os.path.join(workdir, "save")
        os.makedirs(dump_dir)
        os.makedirs(save_dir)

        save_path = os.path.join(save_dir, fixture_name)
        shutil.copy2(fixture_src, save_path)

        try:
            result = subprocess.run(
                ["docker", "run", "--rm",
                 "-v", f"{REPO_ROOT}:/src:ro",
                 "-v", f"{save_dir}:/save:rw",
                 "-v", f"{dump_dir}:/dump:rw",
                 DOCKER_IMAGE,
                 "mgba-lua",
                 EDITOR_ROM_CONTAINER, DUMP_LUA_CONTAINER, FRAMES,
                 "--save", f"/save/{fixture_name}"],
                capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"{fixture_name}: mgba-lua did not exit within {TIMEOUT_SECONDS}s")

        if result.returncode != 0:
            fail(f"{fixture_name}: mgba-lua exited {result.returncode}", result)
        if "dump_after_boot: OK" not in result.stdout:
            fail(f"{fixture_name}: 'dump_after_boot: OK' marker not found", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"{fixture_name}: dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            sram = f.read()

        if len(sram) != 131072:
            fail(f"{fixture_name}: expected 128 KiB FLASH dump, got {len(sram)} bytes "
                 f"(boot-time FLASH detect did not fire — check frame count "
                 f"and editor flash_unlock_prefix)", result)

        detected = pk.detect(sram, lib=lib)
        if detected != expected_game:
            fail(f"{fixture_name}: expected {expected_game.name}, got {detected.name}", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name = pk.get_trainer_name(save)
        finally:
            pk.close(save)

        if name != EXPECTED_TRAINER_NAME:
            fail(f"{fixture_name}: trainer name {name!r} != expected {EXPECTED_TRAINER_NAME!r}", result)

        print(f"  OK  {fixture_name:12s} detect={detected.name:7s} name={name!r}")


def main():
    lib = pk.load_library()
    print(f"running {len(CASES)} Gen-3 fixture round-trip cases:")
    for fixture_name, expected_game in CASES:
        run_case(fixture_name, expected_game, lib)
    print(f"PASS: {len(CASES)}/{len(CASES)} fixtures round-tripped — sidecar load → 128 KiB FLASH dump → ctypes decode")
    return 0


if __name__ == "__main__":
    sys.exit(main())
