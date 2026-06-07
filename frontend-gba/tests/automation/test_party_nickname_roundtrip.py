#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Wave 2A test 3 — party slot 0 nickname round-trip through text_input drill.

Drives the editor into ROW_NICKNAME, presses A to drill into the
text_input scene, types one character, commits with START, returns to
pokemon_edit_scene (which consumes the mailbox result and pushes to
save), exits to main_menu, and triggers Save & Exit.

Asserts the slot 0 nickname grew from "BULBASAUR" to "BULBASAURA"
(adding one 'A' — the text_input grid cursor starts at 'A').

Run from the repo root:
    python3 frontend-gba/tests/automation/test_party_nickname_roundtrip.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_party_nickname_edit_save.lua"
FRAMES = "600"
TIMEOUT_SECONDS = 180

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS
INITIAL_NICKNAME  = b"BULBASAUR"
EXPECTED_NICKNAME = b"BULBASAURA"
INITIAL_SPECIES = 1
ORIGINAL_TRAINER_NAME = "RED"


def fail(msg, result=None):
    print(f"FAIL: {msg}", file=sys.stderr)
    if result is not None:
        print(f"--- stdout (tail) ---\n{result.stdout[-2000:]}", file=sys.stderr)
        print(f"--- stderr (tail) ---\n{result.stderr[-1000:]}", file=sys.stderr)
    sys.exit(1)


def main():
    fixture_src = os.path.join(REPO_ROOT, "libpksave", "tests", "fixtures", FIXTURE_NAME)
    if not os.path.isfile(fixture_src):
        fail(f"fixture missing: {fixture_src}")

    with tempfile.TemporaryDirectory(prefix="pksave-nickname-") as workdir:
        dump_dir = os.path.join(workdir, "dump")
        save_dir = os.path.join(workdir, "save")
        os.makedirs(dump_dir)
        os.makedirs(save_dir)

        save_path = os.path.join(save_dir, FIXTURE_NAME)
        shutil.copy2(fixture_src, save_path)

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
        if "nickname_edit: OK" not in result.stdout:
            fail("'nickname_edit: OK' marker not found", result)
        if "edit: OK" not in result.stdout:
            fail("'edit: OK' not found — pokemon_edit_scene did not construct", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            sram = f.read()
        if len(sram) != 131072:
            fail(f"expected 128 KiB FLASH dump, got {len(sram)}", result)

        lib = pk.load_library()
        detected = pk.detect(sram, lib=lib)
        if detected != EXPECTED_GAME:
            fail(f"detect {detected.name} != {EXPECTED_GAME.name}", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            name = pk.get_trainer_name(save)
            party, count = pk.get_party(save)
        finally:
            pk.close(save)

        print(f"  trainer name:    {name!r}")
        print(f"  party count:     {count}")
        print(f"  slot 0 species:  {party[0].species}")
        print(f"  slot 0 nickname: {bytes(party[0].nickname).rstrip(chr(0).encode())!r}")

        if name != ORIGINAL_TRAINER_NAME:
            fail(f"trainer name mutated: {name!r} != {ORIGINAL_TRAINER_NAME!r}", result)
        if count < 1:
            fail("party emptied", result)
        if party[0].species != INITIAL_SPECIES:
            fail(f"species mutated: {party[0].species} != {INITIAL_SPECIES} — "
                 f"nickname drive should not have cycled species", result)

        actual_nick = bytes(party[0].nickname).rstrip(b'\x00')
        if actual_nick == INITIAL_NICKNAME:
            fail(f"nickname unchanged ({actual_nick!r}); text_input did not "
                 f"deliver result OR pokemon_edit_scene did not consume the "
                 f"mailbox on return", result)
        if actual_nick != EXPECTED_NICKNAME:
            fail(f"nickname {actual_nick!r} != expected {EXPECTED_NICKNAME!r}; "
                 f"the drive script types exactly one 'A' on top of "
                 f"{INITIAL_NICKNAME!r}", result)

    print(f"PASS: party-slot nickname round-trip — UI drove "
          f"{INITIAL_NICKNAME!r} → {EXPECTED_NICKNAME!r} through FLASH; "
          f"species preserved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
