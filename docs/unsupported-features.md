<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 nulvox
-->

# Unsupported game features

This document lists game features that exist in the real save formats but
that `libpksave` does **not** currently expose. Two categories:

1. **Out of scope (v1, by design)** — listed as non-goals in
   [the original product requirements](internal/PRD.md). Editing them is intentionally deferred so that
   v1 ships a small, audit-able feature set with reliable round-trip
   semantics.
2. **In scope but not yet implemented** — gaps in the current code that
   should land before v1 is considered complete.

If you're trying to use libpksave for something and can't find an API for
it, this is the place to check before filing a bug.

---

## 1. Out of scope (by design)

These features are listed as v1 non-goals in the archived product requirements. There is no API for
any of them and the underlying offsets are not parsed. They are also not
re-checksummed beyond what's needed to keep the save valid — the library
preserves their bytes unchanged on flush.

| Feature                              | Affected generations | Notes |
|--------------------------------------|----------------------|-------|
| Pokédex seen / caught flags          | I – V                | Bitfields in trainer block; library round-trips the bytes verbatim but does not parse them. |
| Contest stats (coolness, etc.)       | III, IV              | Per-Pokémon, inside the encrypted PKM block in Gen III/IV. |
| Ribbons                              | III – V              | Per-Pokémon bitfield in the PKM block. |
| Friendship / happiness               | II – V               | Per-Pokémon byte; readable from the raw bytes but not surfaced. |
| Secret Base                          | III (RSE)            | Whole sub-block. |
| Pokéathlon stats                     | IV (HGSS)            | HGSS-only sub-block. |
| Underground items / traps            | IV (DPPt)            | DP/Pt sub-block. |
| Battle Frontier / Pokétch state      | III, IV              | |
| Entralink / Pass Powers / Funfest    | V                    | |
| C-Gear skin                          | V                    | |
| Mystery Gift records                 | III – V              | Some are checksummed separately; library does not touch them. |
| Game-specific event flags            | I – V                | Story / story-flag bytes (e.g. legendary respawn flags). |
| Hall of Fame entries                 | III – V              | Separate save section in Gen III; sub-block in IV/V. |
| Daycare / Battle Box / Battle Subway | II – V               | |
| Mail / Mail attachments              | II – IV              | |
| Daily / hourly RNG seeds             | II – V               | |
| Wonder Trade history                 | V                    | |

The intent for v1 is that **a save edited with libpksave loads cleanly in
the original game and reports no corruption**. Features in this list are
preserved (their bytes pass through unmodified) but not exposed for
editing.

## 2. In scope but not yet implemented (lazy-loaded per generation)

Library capabilities beyond core save detection, checksums, and basic
trainer/party/box/bag round-trips are lazy-loaded when a generation's
frontend work begins (following the pattern in
[`frontend-gba-libpksave-gaps.md`](frontend-gba-libpksave-gaps.md)).
Generations I–III have these helpers implemented; Generations IV–V retain
baseline codecs so the vertical slice can be completed alongside
`frontend-nds`.

### Gen IV & V — Deferred Pokémon fields and lookup helpers

- **Pokémon nickname & OT name (`gen4.c`, `gen5.c`):** `g4_decode_mon` /
  `g4_encode_mon` and `g5_decode_mon` / `g5_encode_mon` currently leave
  `nickname` and `ot_name` empty (`""`).
- **Nature, Ability, and Shininess (`gen4.c`, `gen5.c`):** Not yet populated
  on decode or synced back to PID / Block A on encode.
- **Box Pokémon level / XP (`gen4.c`, `gen5.c`):** 136-byte box Pokémon omit
  the party footer (`0x8C` level), so `g4_decode_box_mon` and
  `g5_decode_box_mon` currently leave `level = 0` rather than deriving it
  from Block A experience via growth-rate tables (as `gen3.c` does).
- **Ability name tables (`names/gen4_abilities.h`, `names/gen5_abilities.h`):**
  Currently 1-entry `"???"` stubs (`TODO(3)`).
- **Stat recalculation & metadata lookups (`recalc.c`, `lookups.c`, `pksave.h`):**
  `pksave_recalc_stats()`, `pksave_species_abilities()`,
  `pksave_move_base_pp()`, `pksave_item_max_stack()`, `pksave_item_pocket()`,
  and `PKSAVE_GENx_*_MAX` name-length constants are implemented for
  Generations I–III and return sentinels (`0` / `PKSAVE_POCKET_NONE`) for
  Generations IV–V.

### Gen I & II — Experience sync on level edit

Party Pokémon store both `level` and `experience`, while Box Pokémon store
`box_level` (`0x03` in Gen I) or `level` (`0x1F` in Gen II) alongside the
3-byte `experience` (`0x0E..0x10` in Gen I, `0x08..0x0A` in Gen II). Editing
`level` currently updates the stored level byte(s) and preserves existing
`experience` bytes unchanged rather than recomputing `experience` from a
per-species growth-rate curve.

### Gen I & II — Bag pockets

Gen I games have a **single** bag pocket in-engine (`POCKET_ITEMS`). Gen II
separates `POCKET_ITEMS`, `POCKET_BALLS`, `POCKET_KEY`, and `POCKET_TM_HM`
(all wired in `libpksave/src/gen2.c`), while Berries are regular items stored
inside `POCKET_ITEMS` (a dedicated `POCKET_BERRIES` was introduced in Gen III).
Mentioned here only so the asymmetry with Gen III+ is not surprising.

### Held item on Gen II reads

Gen II Pokémon do have a held-item byte. It is read into
`pksave_pokemon_t::held_item` by `gen2_get_party`/`gen2_get_box` and
written back on set — this is implemented. (Listed here so reviewers can
confirm the contrast with Gen I, which has no held item.)

### Move shuffle / structure encryption corners

Gen III Pokémon use a 4-permutation reorder of the four 12-byte
sub-structures inside the encrypted PKM block. Gen IV/V use a 24-shuffle.
The current implementation handles all permutations; new permutations
introduced by event Pokémon (rare in practice) may need extra fixtures
to exercise. Track via the test suite if new edge cases appear.

### Box wallpaper / box names

The library does not expose box names or wallpapers in any generation
(structurally trivial — separate offsets per gen — but not on the v1
critical path).

### Trainer card customisation (Gen III+) / Trainer card stars (Gen III)

Not exposed. Stars in Gen III are derived from progression bytes that
would require parsing event-flag-adjacent state.

### Player position / map state

Not exposed. Editing these can place the player out of bounds, so they
are deliberately not on the v1 path even if straightforward.

---

## How to add a feature

1. Read the relevant pret repo
   ([pokered](https://github.com/pret/pokered),
    [pokecrystal](https://github.com/pret/pokecrystal),
    [pokeemerald](https://github.com/pret/pokeemerald), etc.) to find the
    offset and layout.
2. Add the offset constants near the top of the relevant `genN.c`
   (private to that translation unit).
3. Add the read/write helper inside the `genN.c` file.
4. If the feature needs new public API, declare the dispatcher in
   `pksave.h` and `pksave_internal.h`, then wire it through
   `session.c` with the appropriate `#ifdef PKSAVE_GENx` arms.
5. Add a fixture round-trip test in `libpksave/tests/test_genN.c` —
   read a known value, mutate, flush, re-read, assert.
6. If a fixture needs updating, edit
   `libpksave/tests/fixtures/generate_fixtures.py` and regenerate with
   `make -C libpksave fixtures`.
