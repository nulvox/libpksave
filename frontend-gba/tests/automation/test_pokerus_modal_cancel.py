#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Wave 2B test — Pokerus modal opens from IDENTITY/ROW_POKERUS and B cancels
without writing.

Drives into POKEMON_EDIT slot 0, walks the cursor to ROW_POKERUS (8th
row on IDENTITY), presses A to open the modal, presses B to cancel,
then bails out to main_menu without invoking Save & Exit.

Asserts:
- "pokerus: OK" marker fires from the modal ctor.
- "pokerus: cancel" marker fires from the B-press path.
- "edit: OK" markers reappear after the modal closes (proving the
  return-to-POKEMON_EDIT path works without re-entering the modal).
- Decoded fixture's slot 0 Pokerus status is still NEVER (the fixture
  default, strain=0/days=0) — the cancel path must NOT write pokerus.
- Slot 0 species is unchanged (Bulbasaur, id 1) — a sanity check that
  walking the cursor through 7 DOWNs did not silently mutate anything.

The fixture is delivered via sidecar load; mGBA seeds FLASH with the
fixture content at boot, so a "FLASH all 0xFF" assertion (the pattern
used by test_trainer_scene_nav.py with no sidecar) does not apply here.
Instead we decode the post-run FLASH and assert the pokerus byte is
NEVER and species is unchanged.

Run from the repo root:
    python3 frontend-gba/tests/automation/test_pokerus_modal_cancel.py
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
DRIVE_LUA_CONTAINER = "/src/frontend-gba/tests/automation/drive_pokerus_modal_cancel.lua"
FRAMES = "720"
TIMEOUT_SECONDS = 180

FIXTURE_NAME = "rs.sav"
EXPECTED_GAME = pk.Game.RS


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

    with tempfile.TemporaryDirectory(prefix="pksave-pokerus-cancel-") as workdir:
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
        if "pokerus_modal_cancel: OK" not in result.stdout:
            fail("'pokerus_modal_cancel: OK' marker not found", result)
        if "edit: OK" not in result.stdout:
            fail("'edit: OK' not found — pokemon_edit_scene did not construct", result)
        if "pokerus: OK" not in result.stdout:
            fail("'pokerus: OK' not found — pokerus_modal_scene did not "
                 "construct on A-press over ROW_POKERUS", result)
        if "pokerus: cancel" not in result.stdout:
            fail("'pokerus: cancel' marker not found — B-press did not "
                 "route through the modal's cancel path", result)

        dump_path = os.path.join(dump_dir, "sram.bin")
        if not os.path.isfile(dump_path):
            fail(f"dump not produced: {dump_path}", result)

        with open(dump_path, "rb") as f:
            sram = f.read()
        if len(sram) != 131072:
            fail(f"expected 128 KiB FLASH dump, got {len(sram)}", result)

        lib = pk.load_library()

        # Decode the pristine fixture for the baseline pokerus state.
        # The Gen 3 fixture generator packs IVs into the Misc substruct
        # at offset 0, which is the same byte libpksave's pokerus reader
        # uses, so the "initial" state is fixture-dependent rather than
        # universally NEVER.  The load-bearing assertion is post-cancel
        # == pre-edit, not post-cancel == NEVER.
        with open(fixture_src, "rb") as f:
            fixture = f.read()
        if len(fixture) != 131072:
            fail(f"fixture wrong size: {len(fixture)}", result)
        baseline_save = pk.open(fixture, EXPECTED_GAME, lib=lib)
        try:
            baseline_party, _ = pk.get_party(baseline_save)
            baseline_pkrs = pk.get_party_pokerus(baseline_save, 0)
        finally:
            pk.close(baseline_save)

        detected = pk.detect(sram, lib=lib)
        if detected != EXPECTED_GAME:
            fail(f"detect {detected.name} != {EXPECTED_GAME.name} — "
                 f"slot 0 corrupted on cancel path", result)

        save = pk.open(sram, detected, lib=lib)
        try:
            party, count = pk.get_party(save)
            pkrs = pk.get_party_pokerus(save, 0)
        finally:
            pk.close(save)

        print(f"  baseline pokerus: status={pk.PokerusStatus(baseline_pkrs.status).name} "
              f"strain={baseline_pkrs.strain} days={baseline_pkrs.days}")
        print(f"  post-run pokerus: status={pk.PokerusStatus(pkrs.status).name} "
              f"strain={pkrs.strain} days={pkrs.days}")
        print(f"  party[0].species: {party[0].species}")

        if party[0].species != baseline_party[0].species:
            fail(f"species mutated: {party[0].species} != "
                 f"{baseline_party[0].species}; the cancel drive should "
                 f"not have touched species", result)
        if (pkrs.status != baseline_pkrs.status
                or pkrs.strain != baseline_pkrs.strain
                or pkrs.days != baseline_pkrs.days):
            fail(f"pokerus mutated: {pk.PokerusStatus(pkrs.status).name}/"
                 f"{pkrs.strain}/{pkrs.days} != baseline "
                 f"{pk.PokerusStatus(baseline_pkrs.status).name}/"
                 f"{baseline_pkrs.strain}/{baseline_pkrs.days}; "
                 f"the cancel path must not write pokerus", result)

    print("PASS: pokerus modal open+cancel — pokerus unchanged from baseline, "
          "species preserved")
    return 0


if __name__ == "__main__":
    sys.exit(main())
