#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Editor boot rejects a non-Gen-3 save without writing FLASH.

Setup:
  Sidecar save is a 131,072-byte (128 KiB) blob of 0xAB.  That's a
  valid GBA FLASH 128K save *size* but not a valid Gen-3 save format,
  so pksave_detect returns UNKNOWN.  main.cpp's make_initial_scene
  picks boot_error_scene in that case; boot_error halts forever and
  ignores all input.

Drive:
  After boot stabilizes, drive the Save & Exit input sequence used by
  gate J (drive_name_edit_save.lua's terminal navigation): DOWN x4 +
  A, paced at 30-frame intervals.  On main_menu this would walk the
  cursor to Save & Exit and trigger save_scene → flush_to_flash; on
  boot_error every tap is silently ignored.

Differentiating assertion:
  Dump FLASH at shutdown.
   - Size must be 131072 (flash_read_all in main.cpp runs unconditionally,
     so mGBA's FLASH 128K backend is allocated regardless of detect result).
   - Every byte must still be 0xAB.
     * If boot_error_scene halted as designed: no scene transition →
       no save_scene → no flush_to_flash → mGBA's in-memory FLASH
       mirror is byte-for-byte the loaded fixture.
     * If boot_error failed and main_menu took over: the DOWN x4 + A
       sequence would invoke save_scene, which calls
       pksave_save_buffer + flash_write_all_dirty, overwriting the
       region with section-formatted bytes (NOT all 0xAB).

Run from the repo root:
    python3 frontend-gba/tests/automation/test_boot_rejects_bad_save.py
"""

import os
import shutil
import subprocess
import sys
import tempfile

DOCKER_IMAGE = "pksave-test-gba"

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_save_attempt.lua"

# 200 frames is comfortably past the last scheduled tap (frame 180 in
# drive_save_attempt.lua) plus a few frames for the shutdown callback.
FRAMES = "200"
TIMEOUT_SECONDS = 120

# Sentinel byte chosen specifically because it is NOT a valid Gen-3
# section trailer signature (0x08012025) byte and NOT the FLASH
# erased-state byte (0xFF).  Any value the editor writes during a real
# save would clobber some 0xAB bytes, so an all-0xAB dump is strong
# evidence no writes occurred.
SENTINEL_BYTE = 0xAB
SAVE_SIZE = 128 * 1024  # 131072 — FLASH 128K, matches what the editor declares.
FIXTURE_NAME = "bad.sav"


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def main():
    with tempfile.TemporaryDirectory(prefix="pksave-bootrej-") as workdir:
        dump_dir = os.path.join(workdir, "dump")
        save_dir = os.path.join(workdir, "save")
        os.makedirs(dump_dir)
        os.makedirs(save_dir)

        # Stage the bad save: 128 KiB of 0xAB.
        save_path = os.path.join(save_dir, FIXTURE_NAME)
        with open(save_path, "wb") as f:
            f.write(bytes([SENTINEL_BYTE]) * SAVE_SIZE)

        # /save is mounted RW because mCoreLoadSaveFile() opens with
        # O_CREAT|O_RDWR even when mGBA is configured for temporary
        # (in-memory) saves — see docker/test-gba/mgba-lua.c and
        # test_sidecar_save.py's explanation.  /dump is RW so the Lua
        # shutdown callback can write the FLASH region.
        try:
            result = subprocess.run(
                ["docker", "run", "--rm",
                 "-v", f"{REPO_ROOT}:/src:ro",
                 "-v", f"{save_dir}:/save:rw",
                 "-v", f"{dump_dir}:/dump:rw",
                 DOCKER_IMAGE,
                 "mgba-lua",
                 EDITOR_ROM_CONTAINER, DRIVE_LUA_CONTAINER, FRAMES,
                 "--save", f"/save/{FIXTURE_NAME}"],
                capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"mgba-lua did not exit within {TIMEOUT_SECONDS}s")

        if result.returncode != 0:
            fail(f"mgba-lua exited {result.returncode}", result)
        if "boot_reject: OK" not in result.stdout:
            fail("'boot_reject: OK' marker not found", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            dump = f.read()

        print(f"  dump size: {len(dump)} bytes")
        if len(dump) != SAVE_SIZE:
            fail(
                f"expected {SAVE_SIZE}-byte FLASH 128K region "
                f"(flash_read_all runs regardless of detect result), "
                f"got {len(dump)}",
                result,
            )

        # The differentiating assertion: every byte still 0xAB.
        non_sentinel = [(i, b) for i, b in enumerate(dump) if b != SENTINEL_BYTE]
        if non_sentinel:
            sample = [(hex(i), hex(b)) for i, b in non_sentinel[:8]]
            fail(
                f"FLASH was mutated: {len(non_sentinel)} of {len(dump)} bytes "
                f"differ from sentinel 0x{SENTINEL_BYTE:02X}. "
                f"This means boot_error_scene did NOT halt — either the "
                f"editor fell through to main_menu (Save & Exit ran a real "
                f"flush) or some other code path is writing FLASH on a "
                f"corrupt-save boot.  First 8 deltas: {sample}",
                result,
            )

    print(
        f"PASS: bad-save boot rejected; {len(dump)} bytes dumped, "
        f"all 0x{SENTINEL_BYTE:02X} (boot_error_scene halted before any "
        f"FLASH write could occur)"
    )
    return 0


if __name__ == "__main__":
    if shutil.which("docker") is None:
        print("SKIP: docker not on PATH", file=sys.stderr)
        sys.exit(0)
    sys.exit(main())
