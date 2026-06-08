<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# libpksave gaps surfaced by GBA frontend planning

Produced during planning for `frontend-gba` (Ruby, Sapphire, Emerald,
FireRed, LeafGreen — Gen III). Each row is an API the frontend needs that
`libpksave` does not currently expose. Captured here so the rollout
strategy (correction sprint vs. lazy-load per checkpoint) is a single
visible decision rather than a series of in-flight scope creeps.

Date produced: 2026-06-03.

---

## Scoping principle

`frontend-gba` is **an editor utility for players who already know the
game**, not a replica of the game UI. The editor surfaces what's needed
to make valid edits — caps, valid choices, identity strings — and trusts
the user to know the game flavor (type matchups, item descriptions,
ability descriptions, etc.). When weighing whether a `libpksave` API gap
is worth closing, ask: does the user need this to make a *valid* edit,
or only to *recognize* what they're editing? Only the former is on the
critical path.

---

## Critical gaps (block a specific checkpoint)

| ID | Gap | Blocks | Effort | Risk if deferred |
|----|-----|--------|--------|-------------------|
| ~~**L1**~~ | ~~`pksave_recalc_stats(pkm, game)` — recompute `stat[6]` from species + level + nature + EVs + IVs~~ — **Resolved** 2026-06-04 in `libpksave/src/recalc.c` (formula + 25-nature mod table), `libpksave/src/names/gen3_base_stats.h` (413-entry table sourced from pret/pokeruby `src/data/pokemon/base_stats.h`), and `libpksave/src/gen3.c` (PID-derived `nature` / `is_shiny` / `ability` populated on party + box read — prerequisite for the UI flow to work). Public prototype in `pksave.h`. Coverage in `tests/test_gen3_recalc.c` (8 cases: HP/non-HP formula, Adamant/Bold nature mods, multi-species lookup, unknown-species guard) and the extended `tests/test_gen3.c` PID-derived field assertions. | — | — | — |
| ~~**L2**~~ | ~~Verify `pksave_set_party_slot` honors `is_shiny`; add explicit setter if PID-recompute is missing~~ — **Resolved** 2026-06-05 in `libpksave/src/gen3.c`. PID-recompute was missing; `set_party_slot` / `set_box_slot` now call a shared `g3_encode_encrypted_block` helper that searches for a new PID matching `mon->nature` and `mon->is_shiny` (step-of-25 walk, capped at 100k iterations), re-arranges substructs to the new pid%24 layout, and re-encrypts with the new key. Ability bit (iv_word bit 31) is also now writable through `mon->ability` — was bundled because the same write path covers it. Coverage in `tests/test_gen3.c` (`test_rs_party_shiny_toggle_round_trip`, `test_rs_box_unshiny_toggle_round_trip` — both verify nature preservation through the toggle). | — | — | — |
| ~~**L4**~~ | ~~`pksave_species_abilities(game, species_id) → {slot0_id, slot1_id}` — populates the ability cycler with valid choices~~ — **Resolved** 2026-06-05 in `libpksave/src/lookups.c` (switch-on-game lookup returning `{0,0}` for non-Gen-3), `libpksave/src/names/gen3_base_stats.h` (struct widened to 8 bytes with `ability1`/`ability2` columns sourced from pret/pokeruby `src/data/pokemon/base_stats.h`; existing six stat columns re-verified row-for-row against pret with no discrepancies). Public prototype + `pksave_species_abilities_t` struct in `pksave.h`. Coverage in `libpksave/tests/test_gen3_lookups.c` (Bulbasaur single-ability, Pikachu Static, Rattata two abilities, unknown species, OLD_UNOWN placeholder, Egg, non-Gen-3 game). | — | — | — |
| ~~**L5**~~ | ~~`pksave_move_base_pp(game, move_id)` — for PP numeric field cap (×1.6 to allow PP-Up headroom)~~ — **Resolved** 2026-06-05 in `libpksave/src/lookups.c` (switch-on-game lookup returning 0 for non-Gen-3) and `libpksave/src/names/gen3_move_pp.h` (355-entry `uint8_t` table indexed 0..354, sourced from pret/pokeemerald `src/data/battle_moves.h` `.pp` field). Public prototype in `pksave.h`. Coverage in `libpksave/tests/test_gen3_lookups.c` (Pound 35, Thunderbolt 15, Tackle 35, Struggle 1, Splash 40, Hidden Power 15, unknown move, move 0, non-Gen-3 game). | — | — | — |
| ~~**L6**~~ | ~~`pksave_item_max_stack(game, item_id)` — for the slot_edit_scene qty field cap (HMs and key items return 1)~~ — **Resolved** 2026-06-07 in `libpksave/src/lookups.c` (switch-on-game lookup returning 0 for non-Gen-3) and `libpksave/src/names/gen3_item_max_stack.h` (377-entry `uint16_t` table indexed 0..376). Source of truth: pret/pokeemerald `src/data/items.h` `.pocket` + `.importance` fields (`.importance >= 1` → 1; `.pocket == POCKET_BERRIES` → 999; otherwise 99; ID 0 / placeholders → 0). RSE and FRLG agree on every shared ID, so the pokeemerald table covers both. Coverage in `libpksave/tests/test_gen3_lookups.c` (Master Ball 99, Potion 99, HM01 1, TM01 99, Cheri Berry 999, key items 1 cross-game, ITEM_NONE 0, unknown ID 0, placeholder ID 0, non-Gen-3 0 — 10 cases). | — | — | — |
| ~~**L7**~~ | ~~`pksave_item_pocket(game, item_id)` — for cycler scoping inside slot_edit_scene~~ — **Resolved** 2026-06-07 in `libpksave/src/lookups.c` and `libpksave/src/names/gen3_item_pocket.h` (377-entry `uint8_t` table indexed 0..376, sourced from pret/pokeemerald `.pocket` field). Returns the existing `pksave_pocket_t` enum, extended with a `PKSAVE_POCKET_NONE = 0xFF` out-of-band sentinel for ITEM_NONE / unknown IDs. RSE and FRLG agree on per-item pocket assignment, so one canonical pokeemerald-derived table covers all three Gen 3 variants. Coverage in `libpksave/tests/test_gen3_lookups.c` (one test per pocket: Master Ball→BALLS, Potion→ITEMS, TM01/HM01→TM_HM, Cheri Berry→BERRIES, Coin Case+Bicycle→KEY cross-game, ITEM_NONE/unknown/non-Gen-3 → NONE — 9 cases). | — | — | — |
| ~~**L10**~~ | ~~`gen3_encode_char` / `gen3_decode_char` in `libpksave/src/gen3.c:45-61` round-trip only A-Z / a-z / space — digits 0-9 and Gen 3 punctuation silently become `0xFF` (string terminator), truncating names on save~~ — **Resolved** 2026-06-04 in `libpksave/src/gen3.c` (encoder/decoder rewritten to the pret-canonical mapping), `libpksave/tests/fixtures/generate_fixtures.py` (matching gen3_encode + regenerated fixtures), and `frontend-gba/src/widgets/text_input_charset.h` (symbol tab widened to `0-9 ! ? . - , / ' " ` + space). Coverage in `libpksave/tests/test_gen3_charset.c` (canonical-bytes test + keyboard round-trip). | — | — | — |

> IDs are non-contiguous on purpose: **L3** (`pksave_species_types`) was
> dropped during planning because the editor does not surface type
> badges — see the scoping principle above. **L10** was discovered
> during Checkpoint #2 implementation (2026-06-04) when the text_input
> keyboard's charset was gated against what libpksave's Gen 3 encoder
> can round-trip. Closing L10 surfaced a second, larger issue: the
> encoder's pre-existing letter mapping (`A-Z` at 0xA1-0xBA, `a-z` at
> 0xBB-0xD4) was off by 26 bytes vs. the pret-canonical mapping
> (`0-9` at 0xA1-0xAA, punctuation at 0xAB-0xBA, `A-Z` at 0xBB-0xD4,
> `a-z` at 0xD5-0xEE). Synthetic fixtures used the same wrong bytes so
> the round-trip tests passed inside libpksave, but bytes written to
> FLASH would not render correctly on a real Gen III cartridge. The L10
> resolution rewrote the encoder/decoder to canonical and regenerated
> all fixtures; the new `test_gen3_encoder_canonical_bytes` test guards
> against silent regression.

---

## Library completeness gaps — editor v1 does not surface these

These are real holes in `libpksave`'s API surface that this frontend's
editor framing does not need. Tracked here so other consumers (a future
desktop tool, a community frontend, or v2 of this editor) can see them
in one place, and so we don't pretend the library is complete just
because v1 didn't need them.

| ID | Gap | Why the editor skips it | Effort if/when filled |
|----|-----|-------------------------|------------------------|
| **L8** | `pksave_badge_name(game, badge_index) → const char *` — per-game badge name lookup (Hoenn badges differ from Kanto FRLG badges) | Editor renders the badge bitfield as `Badge 1`–`Badge 8` toggles; players know which gym maps to which slot | Small — two parallel tables (Hoenn / Kanto) gated by `pksave_game_t` |
| **L9** | `pksave_nature_modifiers(nature_id) → {boosted_stat, lowered_stat}` — nature's ±10% stat pair | Editor shows nature name only; players know "Adamant" means +Atk −SpA | Small — single 25-entry table; not game-dependent |

---

## Pre-UI library sprint — landed before any `frontend-gba` C++ code

Originally planned as "frontend hardcodes for v1, retrofit at NDS."
Promoted to a pre-UI library sprint instead. Rationale:

- They're small, stable, and shared across Gen III+.
- Doing them once in `libpksave` is cheaper than hardcode-then-retrofit.
- The resulting library is a cleaner foundation for the Gen IV developer
  to build against — they'll discover their own gaps (Gen IV specifics)
  rather than re-discovering ours.

| ID | Helper | Notes |
|----|--------|-------|
| ~~**D1**~~ | ~~`pksave_ev_max_per_stat(game) → uint16_t`, `pksave_ev_total_max(game) → uint16_t`~~ | ~~252 / 510 in Gen III+; different units in Gen I/II stat-exp world~~ — **Resolved** 2026-06-04 in `libpksave/src/limits.c`; tested in `tests/test_limits.c`. Gen I/II `ev_total_max` returns `0` as a sentinel meaning "no total cap". |
| ~~**D2**~~ | ~~`pksave_iv_max(game) → uint8_t`~~ | ~~31 in Gen III+; 15 in Gen I/II DV world~~ — **Resolved** 2026-06-04 in `libpksave/src/limits.c`; tested in `tests/test_limits.c`. |
| ~~**D3**~~ | ~~`PKSAVE_GEN3_TRAINER_NAME_MAX`, `PKSAVE_GEN3_NICKNAME_MAX`, `PKSAVE_GEN3_OT_NAME_MAX` — public `#define`s in `pksave.h`; per-gen equivalents added when those frontends land~~ | ~~Frontends pull from the header; no magic numbers in bridge code~~ — **Resolved** 2026-06-04 in `libpksave/include/pksave.h`; tested in `tests/test_limits.c`. |

---

## Out of v1 scope

Not gaps to close — features deliberately not in the v1 surface per the
scoping principle ("editor for players who know the game"). Listed here
so they aren't re-discovered as gaps during a later checkpoint.

- **Item descriptions** — game flavor; players know what a Potion does
- **Species type lookups** — game flavor; players know Bulbasaur is Grass/Poison
- **Move type / category lookups** — game flavor
- **Ability descriptions** — game flavor (ability *names* are still surfaced via L4)
- **Box names / wallpapers** — Gen III allows renaming boxes; defer to v2

---

## Rollout strategy — chosen

**Hybrid: pre-UI sprint for D1–D3, lazy-load critical helpers per
checkpoint, distribute completeness helpers to future consumers.**

Sequence:

1. **Sprint 0 (pre-UI):** D1, D2, D3 land in `libpksave` with Unity tests.
   No `frontend-gba` code yet. Establishes a complete-enough library
   surface before the Gen IV developer needs it.
2. **Checkpoint #3 kickoff:** L1, L2, L4, L5 land in `libpksave` as the
   first tasks of the Pokemon editor checkpoint. Each ships with the UI
   that consumes it.
3. **Checkpoint #4 kickoff:** L6, L7 land at the start of the Bag
   checkpoint, same shape.
4. **L8, L9:** left open. Filled by whichever frontend (this v2, NDS,
   future tools) actually needs to surface them. May never be filled —
   acceptable.
5. **The Gen IV developer:** expected to discover Gen IV / Gen V specific
   gaps as they go and file them against this matrix. The pre-UI sprint
   (D1–D3) means they inherit a coherent library surface; the lazy-load
   pattern is the precedent for how they extend it.

### Open gap tracker

Tick the box when the gap is resolved; on resolution, also strike
through the row in the table above and add a **Resolved** YYYY-MM-DD
note pointing at the file(s) where it landed (see D1–D3 for the
pattern).

- [x] **L1** — `pksave_recalc_stats` (resolved 2026-06-04; canonical Gen 3 stat formula + nature mod + full 413-entry base-stats table in `libpksave/src/recalc.c` + `names/gen3_base_stats.h`; bundled with PID-derivation of `nature`/`is_shiny`/`ability` on Gen 3 reads in `gen3.c` since recalc is unusable without it)
- [x] **L2** — verify / fix `pksave_set_party_slot` shiny round-trip (resolved 2026-06-05; PID-recompute + substruct re-arrange landed in `gen3.c`'s shared encrypted-block encoder; `mon->ability` now also writable)
- [x] **L4** — `pksave_species_abilities` (resolved 2026-06-05; `gen3_base_stats_t` widened to 8 bytes with pret-sourced `ability1`/`ability2` columns in `names/gen3_base_stats.h`; lookup lives in `src/lookups.c` and switches on game)
- [x] **L5** — `pksave_move_base_pp` (resolved 2026-06-05; new 355-entry `names/gen3_move_pp.h` table sourced from pret/pokeemerald `src/data/battle_moves.h`; lookup lives in `src/lookups.c`)
- [x] **L6** — `pksave_item_max_stack` (resolved 2026-06-07; 377-entry `names/gen3_item_max_stack.h` derived from pret/pokeemerald `.pocket` + `.importance`; lookup in `src/lookups.c`)
- [x] **L7** — `pksave_item_pocket` (resolved 2026-06-07; 377-entry `names/gen3_item_pocket.h` from pret/pokeemerald `.pocket`; reuses existing `pksave_pocket_t` enum extended with `PKSAVE_POCKET_NONE = 0xFF` sentinel)
- [ ] **L8** — `pksave_badge_name` (deferred; future consumer)
- [ ] **L9** — `pksave_nature_modifiers` (deferred; future consumer)
- [x] **L10** — Gen 3 encoder/decoder full charset (resolved 2026-06-04; canonical mapping landed in `libpksave/src/gen3.c`, fixtures regenerated, keyboard `sym` tab widened to digits + 8 punctuation marks)

---

## Notes

- L1 is the largest single piece of work in this matrix and has been
  pre-approved as its own step inside Checkpoint #3.
- Adding any of these helpers should follow the existing libpksave
  convention: a `const`-data table in `names/gen3_*.h` guarded by
  `#ifdef PKSAVE_GEN3`, with a Unity test in `tests/test_gen3.c`.
- When a helper lands, this file should be updated to mark the row
  resolved (strikethrough or move to a "Closed" section).
