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
| **L1** | `pksave_recalc_stats(pkm, game)` — recompute `stat[6]` from species + level + nature + EVs + IVs | Checkpoint #3 (Pokemon editor STATS tab) | Medium — Gen III base-stats table (387 species × 6 stats) + nature multiplier table + formula | Stale cached stats in UI after edits; confusing UX |
| **L2** | Verify `pksave_set_party_slot` honors `is_shiny`; add explicit setter if PID-recompute is missing | Checkpoint #3 (Identity tab Shiny toggle) | Small if already supported (write a test); Medium if PID-recompute logic missing | Shiny becomes display-only; toggle is a no-op |
| **L4** | `pksave_species_abilities(game, species_id) → {slot0_id, slot1_id}` — populates the ability cycler with valid choices | Checkpoint #3 (Identity tab ability field) | Small — parallel table in `names/gen3_species.h` | Cycler walks all abilities; can pick invalid ones (game may reject on save) |
| **L5** | `pksave_move_base_pp(game, move_id)` — for PP numeric field cap (×1.6 to allow PP-Up headroom) | Checkpoint #3 (Moves tab) | Small — parallel table in `names/gen3_moves.h` | Frontend hardcodes caps; drifts from format truth |
| **L6** | `pksave_item_max_stack(game, item_id)` — for the slot_edit_scene qty field cap (HMs and key items return 1) | Checkpoint #4 (Bag) | Small — parallel `const uint16_t` table in `names/gen3_items.h` | Can't enforce per-item caps; user can set qty=99 on HM01 and corrupt save logic |
| **L7** | `pksave_item_pocket(game, item_id)` — for cycler scoping inside slot_edit_scene | Checkpoint #4 (Bag) | Small — parallel table | Cycler walks all items; user can place Potion in Balls pocket; game may re-sort or misbehave on reload |

> IDs are non-contiguous on purpose: **L3** (`pksave_species_types`) was
> dropped during planning because the editor does not surface type
> badges — see the scoping principle above.

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
| **D1** | `pksave_ev_max_per_stat(game) → uint16_t`, `pksave_ev_total_max(game) → uint16_t` | 252 / 510 in Gen III+; different units in Gen I/II stat-exp world |
| **D2** | `pksave_iv_max(game) → uint8_t` | 31 in Gen III+; 15 in Gen I/II DV world |
| **D3** | `PKSAVE_GEN3_TRAINER_NAME_MAX`, `PKSAVE_GEN3_NICKNAME_MAX`, `PKSAVE_GEN3_OT_NAME_MAX` — public `#define`s in `pksave.h`; per-gen equivalents added when those frontends land | Frontends pull from the header; no magic numbers in bridge code |

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

---

## Notes

- L1 is the largest single piece of work in this matrix and has been
  pre-approved as its own step inside Checkpoint #3.
- Adding any of these helpers should follow the existing libpksave
  convention: a `const`-data table in `names/gen3_*.h` guarded by
  `#ifdef PKSAVE_GEN3`, with a Unity test in `tests/test_gen3.c`.
- When a helper lands, this file should be updated to mark the row
  resolved (strikethrough or move to a "Closed" section).
