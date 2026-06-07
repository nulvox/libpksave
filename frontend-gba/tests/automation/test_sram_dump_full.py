#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Test 4: Lua dumps the *entire* SRAM/FLASH region the running ROM declared,
not just the first 16 bytes. The dump size is detected dynamically by the
Lua side (no host-side hardcoding of 32768 or 131072) and asserted here.

Why dynamic
-----------
The editor ROM may declare SRAM 32 KiB (Track A pending) or FLASH 128 KiB
(Track A landed). This test must pass in either world. It does so by:

  1. Letting `dump_sram_full.lua` detect the region size via mirror probe
     (write a marker, find the lowest offset whose byte wraps).
  2. Parsing the `dump_full: size=N` line that the Lua side emits.
  3. Asserting the on-disk dump file is exactly N bytes.
  4. Sanity-gating N against the set of legal GBA save sizes
     {32768, 65536, 131072} so a bogus probe result fails loudly.

Content is not asserted — without a sidecar save (Track C), fresh
SRAM/FLASH is uniform 0xFF or 0x00 depending on detection, neither of
which carries useful structure.

The smaller `test_sram_dump.py` (16 bytes) is intentionally kept alive
as the tracer-bullet test; this file is the full-region successor.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_sram_dump_full.py
"""

import os
import re
import subprocess
import sys
import tempfile

DOCKER_IMAGE = "pksave-test-gba"

_HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", ".."))
EDITOR_ROM_HOST = os.path.join(REPO_ROOT, "frontend-gba", "pksave-gba.gba")
EDITOR_ROM_CONTAINER = "/src/frontend-gba/pksave-gba.gba"
DUMP_LUA_CONTAINER = "/src/frontend-gba/tests/automation/dump_sram_full.lua"

# Frames: must give the ROM time to alloc its savegame on first SRAM access.
# 30 is more than enough; the warm-up read happens on the first script tick.
FRAMES = "30"
TIMEOUT_SECONDS = 120  # 128 KiB via read8 loop takes ~1–2s; container startup eats more.

LEGAL_REGION_SIZES = {32768, 65536, 131072}


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-2000:]}", file=sys.stderr)
    sys.exit(1)


def main():
    if not os.path.isfile(EDITOR_ROM_HOST):
        fail(f"editor ROM not found: {EDITOR_ROM_HOST} (run `make gba`?)")

    with tempfile.TemporaryDirectory(prefix="pksave-sram-full-") as dump_dir:
        try:
            result = subprocess.run(
                ["docker", "run", "--rm",
                 "-v", f"{REPO_ROOT}:/src:ro",
                 "-v", f"{dump_dir}:/dump:rw",
                 DOCKER_IMAGE,
                 "mgba-lua", EDITOR_ROM_CONTAINER, DUMP_LUA_CONTAINER, FRAMES],
                capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            )
        except FileNotFoundError:
            fail("docker not found on PATH")
        except subprocess.TimeoutExpired:
            fail(f"mgba-lua did not exit within {TIMEOUT_SECONDS}s")

        if result.returncode != 0:
            fail(f"mgba-lua exited {result.returncode}", result)
        if "dump_full: OK" not in result.stdout:
            fail("'dump_full: OK' marker not found", result)

        # Parse the size= line the Lua side prints.
        m = re.search(r"dump_full:\s*size=(\d+)", result.stdout)
        if not m:
            fail("no 'dump_full: size=N' line in stdout", result)
        reported_size = int(m.group(1))

        if reported_size not in LEGAL_REGION_SIZES:
            fail(f"reported size {reported_size} not in legal GBA save set "
                 f"{sorted(LEGAL_REGION_SIZES)}", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump file not produced: {dump_path}", result)

        on_disk_size = os.path.getsize(dump_path)
        if on_disk_size != reported_size:
            fail(f"on-disk size {on_disk_size} != reported size {reported_size}",
                 result)

        # Quick content sanity log — without a fixture loaded the bytes are
        # uniform (0xFF for fresh SRAM/FLASH; 0x00 if the savegame was
        # zero-initialised), and that's fine.
        with open(dump_path, "rb") as f:
            data = f.read()
        head = " ".join(f"{b:02x}" for b in data[:16])
        unique = len(set(data))
        size_kib = on_disk_size // 1024
        print(f"PASS: dumped {on_disk_size} bytes ({size_kib} KiB) of SRAM/FLASH "
              f"via Lua → file → Python; head={head}; unique-bytes={unique}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
