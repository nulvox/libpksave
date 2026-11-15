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

## 2. In scope but not yet implemented (current limitations & gaps)

Library capabilities beyond core save detection, checksums, and basic
trainer/party/box/bag round-trips are lazy-loaded when a generation's
frontend work begins (following the pattern in
[`frontend-gba-libpksave-gaps.md`](frontend-gba-libpksave-gaps.md)).
Below are the current technical limitations across the public struct
surface and each generation's codec.

### Cross-generation API & struct limitations (`pksave.h`)

- **English ASCII character subset:** Name codecs across all generations only
  round-trip basic English ASCII (`A-Z`, `a-z`, `0-9`, space, and basic
  punctuation). Japanese kana/kanji, accented European letters, and
  multi-character cartridge glyphs (`PK`, `MN`, `♂`, `♀`) decode as `'?'` and
  encode as string terminators.
- **No slot deletion, compaction, or Party↔Box move helpers:**
  `pksave_set_party_slot`, `pksave_set_box_slot`, and `pksave_set_item`
  overwrite an existing slot or append at the end of the active count; there
  is no API to remove and compact middle slots in packed arrays or transfer a
  Pokémon between Party and PC Boxes.
- **Explicit `pksave_recalc_stats()` call required before party writes:**
  `pksave_set_party_slot()` serializes `mon->stat[0..5]` as supplied by the
  caller; callers editing level, species, IVs/DVs, EVs, or nature must call
  `pksave_recalc_stats()` first if they want updated party stats written to
  the save.

### Gen I–III unexposed game features & remaining edge cases

Beyond the general v1 non-goals table in Section 1, the following
generation-specific save blocks and fields are preserved in-place on flush
but not surfaced through `pksave.h`:

- **Gen I (`gen1.c`):** Pokédex owned/seen bitfields, PC item box
  (`wNumBoxItems`), Daycare Pokémon (`wDayCareMon`), Pikachu friendship &
  beach minigame data (`YELLOW`), Safari Zone state, Hall of Fame records
  (Bank 0), and story/event flags.
- **Gen II (`gen2.c`):** Pokédex / Unown Dex flags, Pokémon friendship / egg
  cycles (`0x1B`) and caught data (`0x1D..0x1E`), PC item box (`wNumPCItems`),
  Mom's savings (`wMomsMoney`), Casino coins (`wCoins`), Daycare Pokémon,
  Mail messages (`sPartyMail` / `sMailboxes`), Kurt's Apricorns, Pokégear
  phone contacts, Mystery Gift / Mobile Adapter records, Battle Tower state,
  and Hall of Fame records. (Note: Gold/Silver's fragmented backup save
  across Banks 0/1/3 is not mirrored on flush; Crystal's contiguous backup
  save at `0x1209..0x1D82` *is* mirrored and re-checksummed on flush.)
- **Gen III (`gen3.c`):**
  - Unexposed SaveBlock1 / SaveBlock2 / PKM fields: Pokédex flags, Pokémon
    friendship (`Growth+9`), PP Up bonus bitfield (`Growth+8`), Contest stats
    (`EVs+6..11`), Ribbons & obedience bitfield (`Misc+8..11`), markings,
    Pokérus caught/met location & Poké Ball (`Misc+1..3`), PC item storage,
    Pokéblocks, Berry trees, Secret Bases, TV shows / PokéNews / Dewford
    trends, Mauville Old Man, Contest winners, Daycare, Mail, Battle Frontier
    / Trainer Hill / Walda phrase, Roamer, Enigma Berry, Mystery Gift /
    e-Reader Trainer, and Hall of Fame sections.
  - **PID search ignores gender & Unown form:** When `g3_encode_encrypted_block()`
    searches for a new PID to satisfy a changed `nature` or `is_shiny`, it
    does not constrain the low byte (`PID & 0xFF`, which determines gender
    against the species gender threshold) or Unown letter form.

### Gen IV & V limitations (`gen4.c`, `gen5.c` — synthetic-fixture baseline)

Generations IV and V currently ship synthetic-fixture baseline codecs; the
following cartridge-accuracy and feature gaps are deferred until NDS work
begins:

- **Gen IV proprietary character encoding (`gen4.c`):** Real Gen IV cartridges
  use a proprietary 16-bit glyph index table (not Unicode UTF-16LE), whereas
  `gen4.c` currently reads and writes raw ASCII codepoints in 16-bit words to
  match `generate_fixtures.py`.
- **HGSS Kanto badges (`gen4.c`):** `gen4_get_trainer` / `gen4_set_trainer`
  currently read and write a single badge byte (`G4_BADGES_OFF`), so the
  second byte storing HGSS's 8 Kanto badges is not yet wired into the high
  byte of `pksave_trainer_t::badges`.
- **Party tail encryption & Gen V party struct size (`gen4.c`, `gen5.c`):**
  On real Gen IV and V cartridges, the party tail (`0x88..0xEB` in Gen IV,
  `0x88..0xDB` in Gen V — containing level, status, current HP, and calculated
  stats) is encrypted with a PID-seeded LCRNG, and Gen V party Pokémon are
  220 bytes (not 236 bytes). `gen4.c` and `gen5.c` currently read/write
  `0x8C..0x9A` as unencrypted plaintext and use 236-byte party slots for both.
- **Storage block & sub-block CRC tables (`gen4.c`, `gen5.c`):**
  `gen4_flush()` only recomputes the General Block footer CRC-16-CCITT (not
  the separate Storage Block footer CRC-16-CCITT after box edits). `gen5.c`
  uses a synthetic single-block layout (`G5_PC_BOX_BASE = 0x2000U`, single
  footer CRC) rather than Gen V's ~70 per-subblock CRCs and master CRC table.
- **Unwired Pokémon fields (`gen4.c`, `gen5.c`):** `nickname`, `ot_name`,
  `nature`, `ability`, `is_shiny`, and box `level` (from Block A `experience`)
  are not decoded or encoded yet.
- **Deferred lookup & recalc tables (`recalc.c`, `lookups.c`, `names/`):**
  `gen4_abilities.h` and `gen5_abilities.h` are 1-entry `"???"` stubs, and
  `pksave_recalc_stats()`, `pksave_species_abilities()`,
  `pksave_move_base_pp()`, `pksave_item_max_stack()`, `pksave_item_pocket()`,
  and `PKSAVE_GEN4_*` / `PKSAVE_GEN5_*` name-length constants are not yet
  wired.

### Gen I & II — Bag pockets (in-game design, not a library gap)

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
