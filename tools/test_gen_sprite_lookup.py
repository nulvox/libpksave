#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 gbsdk contributors
"""
Unit tests for tools/gen_sprite_lookup.py.

Standalone script — no pytest. Run from anywhere:

    python3 tools/test_gen_sprite_lookup.py

Exits 0 on success, 1 on the first failed assertion.

Background: see docs/frontend-gba-plan.md §"Checkpoint #6 — Sprites" follow-up
"Per-species render branch". The generator turns whatever `species_NNN.bmp`
files happen to live in `frontend-gba/graphics/sprites/` (extracted via
`tools/extract_sprites.py`) into a `species_sprite_lookup.cpp` that maps a
species id to the corresponding `bn::sprite_item*`, or nullptr when no
sprite was extracted. The pokemon editor's right panel uses the lookup to
pick "real sprite if available, else silhouette" per species.
"""

import os
import re
import sys
import tempfile
import traceback

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

import gen_sprite_lookup as gsl  # noqa: E402


def _touch(path: str) -> None:
    """Create an empty file at `path` (parent dir must exist)."""
    open(path, "wb").close()


def _populate(dir_path: str, species_ids):
    """Drop one `species_NNN.bmp` + sidecar JSON per id into `dir_path`."""
    for sid in species_ids:
        _touch(os.path.join(dir_path, f"species_{sid:03d}.bmp"))
        _touch(os.path.join(dir_path, f"species_{sid:03d}.json"))


# ── tracer bullet ────────────────────────────────────────────────────────────
# Empty sprites dir (no `make gba SPRITES_ROM=...` was ever run) must
# still yield a compilable .cpp whose lookup returns nullptr for every
# species — i.e. the editor falls back to the silhouette everywhere.

def test_empty_dir_emits_lookup_that_returns_nullptr():
    with tempfile.TemporaryDirectory() as td:
        sprites_dir = os.path.join(td, "sprites")
        os.makedirs(sprites_dir)
        out_path = os.path.join(td, "species_sprite_lookup.cpp")
        gsl.generate(sprites_dir, out_path)
        with open(out_path, "r", encoding="utf-8") as f:
            cpp = f.read()
    assert "species_sprite_for" in cpp, "function signature missing"
    assert "return nullptr;" in cpp, "default branch must return nullptr"
    assert "case " not in cpp, \
        f"empty dir should emit zero case lines; got:\n{cpp}"


def test_single_species_emits_one_case():
    """A single `species_001.bmp` yields exactly one case returning
    `&bn::sprite_items::species_001`. The naming convention (zero-padded
    3-digit id) matches what `extract_sprites.py` writes, so the bmp filename
    is the contract between the two tools.
    """
    with tempfile.TemporaryDirectory() as td:
        sprites_dir = os.path.join(td, "sprites")
        os.makedirs(sprites_dir)
        _populate(sprites_dir, [1])
        out_path = os.path.join(td, "species_sprite_lookup.cpp")
        gsl.generate(sprites_dir, out_path)
        with open(out_path, "r", encoding="utf-8") as f:
            cpp = f.read()
    assert "case 1:" in cpp, f"missing `case 1:`:\n{cpp}"
    assert "bn::sprite_items::species_001" in cpp, \
        f"missing species_001 sprite_item ref:\n{cpp}"
    case_lines = [ln for ln in cpp.splitlines() if re.match(r"\s*case \d+:", ln)]
    assert len(case_lines) == 1, \
        f"expected 1 case, got {len(case_lines)}:\n{cpp}"


def test_ignores_decoys_in_sprites_dir():
    """The sprites dir is shared with butano's grit pipeline, so it
    also contains JSON sidecars (one per BMP) and may contain the
    generic silhouette.bmp alongside the extracted species ones. None
    of those should produce a switch case — only `species_NNN.bmp`.
    """
    with tempfile.TemporaryDirectory() as td:
        sprites_dir = os.path.join(td, "sprites")
        os.makedirs(sprites_dir)
        _populate(sprites_dir, [7])  # the only legit entry
        _touch(os.path.join(sprites_dir, "silhouette.bmp"))
        _touch(os.path.join(sprites_dir, "silhouette.json"))
        _touch(os.path.join(sprites_dir, "README.txt"))
        _touch(os.path.join(sprites_dir, "species_garbage.bmp"))  # bad id
        _touch(os.path.join(sprites_dir, "species_001.png"))      # wrong ext
        out_path = os.path.join(td, "species_sprite_lookup.cpp")
        gsl.generate(sprites_dir, out_path)
        with open(out_path, "r", encoding="utf-8") as f:
            cpp = f.read()
    case_lines = [ln for ln in cpp.splitlines() if re.match(r"\s*case \d+:", ln)]
    assert len(case_lines) == 1, \
        f"decoys leaked into output; cases:\n{case_lines}\n{cpp}"
    assert "case 7:" in cpp, f"missing legitimate species_007:\n{cpp}"
    # Doc comment mentions "silhouette" — only outlaw it from the
    # generated logic (sprite_items refs + includes).
    assert "sprite_items::silhouette" not in cpp, \
        f"silhouette must never be a lookup target:\n{cpp}"
    assert "bn_sprite_items_silhouette.h" not in cpp, \
        f"silhouette header must not be included:\n{cpp}"


def test_multiple_species_sorted_ascending():
    """Cases are emitted in ascending species-id order. Stable output
    matters for diffability and for cache-friendly fall-through patterns
    on the GBA; readdir order isn't guaranteed across filesystems.
    """
    with tempfile.TemporaryDirectory() as td:
        sprites_dir = os.path.join(td, "sprites")
        os.makedirs(sprites_dir)
        _populate(sprites_dir, [25, 1, 150, 4])
        out_path = os.path.join(td, "species_sprite_lookup.cpp")
        gsl.generate(sprites_dir, out_path)
        with open(out_path, "r", encoding="utf-8") as f:
            cpp = f.read()
    case_pat = re.compile(r"case (\d+):")
    found_ids = [int(m.group(1)) for m in case_pat.finditer(cpp)]
    assert found_ids == sorted(found_ids), \
        f"cases not sorted: {found_ids}"
    assert found_ids == [1, 4, 25, 150], f"unexpected case set: {found_ids}"


def test_missing_sprites_dir_yields_empty_lookup():
    """Sprites dir doesn't exist — the default build path before anyone
    runs `make gba SPRITES_ROM=...`. Generator must not crash; output is
    the same as the empty-dir case so the butano build keeps working.
    """
    with tempfile.TemporaryDirectory() as td:
        sprites_dir = os.path.join(td, "does", "not", "exist")
        out_path = os.path.join(td, "species_sprite_lookup.cpp")
        gsl.generate(sprites_dir, out_path)
        with open(out_path, "r", encoding="utf-8") as f:
            cpp = f.read()
    assert "case " not in cpp, f"missing dir should yield zero cases:\n{cpp}"
    assert "return nullptr;" in cpp


def test_output_is_deterministic():
    """Same inputs must produce byte-identical output. `make`'s
    timestamp logic recompiles the .cpp whenever its mtime advances;
    making the content stable means edits-then-revert don't ripple
    through the rest of the build.
    """
    with tempfile.TemporaryDirectory() as td:
        sprites_dir = os.path.join(td, "sprites")
        os.makedirs(sprites_dir)
        _populate(sprites_dir, [12, 7, 99, 3])
        out_path = os.path.join(td, "species_sprite_lookup.cpp")
        gsl.generate(sprites_dir, out_path)
        with open(out_path, "rb") as f:
            first = f.read()
        gsl.generate(sprites_dir, out_path)
        with open(out_path, "rb") as f:
            second = f.read()
    assert first == second, "generator output drifted between runs"


def test_interface_contract():
    """Signature + namespace are pinned: `pokemon_edit_scene.cpp` calls
    `pksave_gba::sprite_lookup::species_sprite_for(int)` and expects
    a `const bn::sprite_item*`. Drift here is a compile-time break in
    the editor.
    """
    with tempfile.TemporaryDirectory() as td:
        sprites_dir = os.path.join(td, "sprites")
        os.makedirs(sprites_dir)
        out_path = os.path.join(td, "species_sprite_lookup.cpp")
        gsl.generate(sprites_dir, out_path)
        with open(out_path, "r", encoding="utf-8") as f:
            cpp = f.read()
    assert "namespace pksave_gba" in cpp
    assert "namespace sprite_lookup" in cpp
    # Tolerate whitespace flex but pin the type, name, and signature shape.
    assert re.search(
        r"const\s+bn::sprite_item\s*\*\s*species_sprite_for\s*\(\s*int\s+\w+\s*\)",
        cpp), f"signature drifted:\n{cpp}"


def main() -> int:
    tests = [v for k, v in globals().items() if k.startswith("test_")]
    failures = []
    for t in tests:
        try:
            t()
            print(f"  PASS  {t.__name__}")
        except Exception:
            print(f"  FAIL  {t.__name__}")
            traceback.print_exc()
            failures.append(t.__name__)
    if failures:
        print(f"\n=== {len(failures)} test(s) FAILED ===")
        return 1
    print(f"\n=== {len(tests)} test(s) PASSED ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
