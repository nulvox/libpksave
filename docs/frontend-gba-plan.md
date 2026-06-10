<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# `frontend-gba` execution plan

Implementation plan for AGENT_PROMPT.md item #4: the Gen III front-end
for Ruby, Sapphire, Emerald, FireRed, and LeafGreen. SDK: butano on
devkitARM. `libpksave` is linked as a static C library; C++ calls it
through an `extern "C"` bridge.

Companion docs:

- `docs/frontend-gba-libpksave-gaps.md` — the library gaps surfaced by
  this plan and the rollout strategy for closing them.
- `docs/frontend-gba-harness-plan.md` — mGBA-Lua automation harness
  reference: what's shipped, the helper patterns, how to add a new test.
- `PRD.md` / `SPEC.md` / `AGENT_PROMPT.md` — feature requirements,
  technical spec, and the original build order.

Date produced: 2026-06-03. Last status refresh: 2026-06-10 (CP#6 per-species render branch closed).

---

## Where to pick up (status 2026-06-07)

**Shipped:**

- **Sprint 0 (library pre-UI)** — D1, D2, D3 closed.
- **Checkpoint #1** — boot, detect, error scene, main-menu shell.
- **Checkpoint #2** — trainer-screen edit + Save & Exit + text-input
  keyboard.
- **CP#3 library kickoff** — L1, L2, L4, L5 closed (recalc, shiny
  round-trip via PID recompute, ability lookup, move PP). L10 (charset)
  also closed mid-CP#2.
- **CP#3 frontend** — `widgets/cycler`, `scenes/party_scene`,
  `scenes/pokemon_edit_scene` (IDENTITY / STATS / MOVES tabs, Pokerus
  sub-modal). All harness round-trips green.
- **CP#4 library kickoff** — L6, L7 closed (2026-06-07). 19 new Unity
  cases in `test_gen3_lookups.c` covering per-pocket stack caps + pocket
  assignments sourced from pret/pokeemerald `src/data/items.h`.
- **CP#4 frontend** — `scenes/bag_scene` (pocket-tabbed slot list),
  `scenes/slot_edit_scene` (item cycler scoped via L7 + qty
  numeric_field capped via L6).  Two new harness tests
  (`test_bag_scene_nav.py`, `test_bag_slot_edit_roundtrip.py`).
- **CP#5 library** — L11 closed (Gen 3 box-mon level/XP decode +
  encode); 413-entry per-species growth-rate table sourced from
  pret/pokeemerald `species_info.h`; XP↔level math from pret's
  `experience_tables.h` formulas.  Fixture generator updated to seed
  canonical XP.
- **CP#5 frontend** — `scenes/box_scene` (linear 30-slot list_view;
  SELECT cycles boxes wrapping; A drills into pokemon_edit_scene via
  new `pending_box_index` / `pending_box_slot` pair on the bridge,
  mirroring the bag handoff pattern).  `pokemon_edit_scene` branches
  `load_from_save` / `push_to_save` / B-return on a new `edit_source`
  enum (PARTY vs BOX); ROW_POKERUS guarded as no-op when source==BOX
  (deferred follow-up).  Three new harness tests
  (`test_box_scene_nav.py`, `test_box_species_roundtrip.py`,
  `test_box_stats_level_roundtrip.py`).
- **CP#6 algorithmic core + silhouette + auto-discovery (2026-06-09)** —
  `tools/extract_sprites.py` ships with 35 unit tests (`make test-tools`):
  variant detection, GBA pointer read, LZ77 (LZ10) decompress, 4bpp
  untile, BGR555 → RGB888, indexed BMP writer, `extract_species`,
  `extract_all` orchestration with idempotency, plus auto-discovery
  scanners that locate `gMonFrontPicTable` / `gMonPaletteTable` by
  structural signature (no hardcoded variant addresses needed —
  region-agnostic).  `tools/make_silhouette.py` one-shots a
  procedural 64×64 silhouette BMP into
  `frontend-gba/graphics/silhouette.bmp`; `pokemon_edit_scene`
  renders it on the IDENTITY/MOVES right panel via butano
  `bn::sprite_items::silhouette`.  Root `Makefile` wires
  `make gba SPRITES_ROM=/path/to/gen3.gba` to invoke the extractor
  before docker.  Format note: silhouette + extracted sprites both use
  4bpp indexed BMP (not PNG as originally planned — butano 18.5's grit
  only consumes BMP).
- **mGBA-Lua automation harness** — POC + five hardening rounds
  shipped. `make test-gba` is a hard merge gate covering **29 tests**:
  per-variant fixture round-trip (RS / Emerald / FRLG), trainer-name
  round-trip per variant, TID numeric round-trip, main-menu cursor
  wrap, boot rejects malformed save, party / pokemon-edit / pokerus /
  stats / moves round-trips, bag nav + bag slot-edit round-trip,
  box-scene nav + box species round-trip, plus the supporting smoke /
  detect / dump / sidecar / key-input tests.
  See `docs/frontend-gba-harness-plan.md`.

**Next on your plate:** **Checkpoint #6 is closed.** The extractor
auto-discovers the sprite/palette tables (no hardcoded variant addresses
needed) and `tools/gen_sprite_lookup.py` emits the per-species switch
that drives the pokemon-editor right panel ("real sprite if extracted,
else silhouette").  See "Checkpoint #6 — Sprites (Option C)" below.
Remaining items (3-variant real-ROM end-to-end test, party/box right
panels) are out-of-scope follow-ups, not blockers.

**Open library gaps that block later checkpoints:** none.
L8, L9 deferred indefinitely.

**Before you write code, read in this order:**

1. This document, top to bottom — the locked decisions in
   §"Locked architectural decisions" are contracts.
2. `docs/frontend-gba-harness-plan.md` — every CP#3 scene you ship
   needs a `make test-gba` test, modeled after the patterns there.
   Skim the "Patterns" section first; the rest is reference.
3. `docs/frontend-gba-libpksave-gaps.md` — confirm the API you'll
   consume is closed. If you need something new, file it there before
   adding it.
4. The §"Patterns and lessons" section at the bottom of this doc —
   pitfalls and conventions discovered during CP#1, CP#2, and harness
   work. The most load-bearing one is the **list_view edge-consumption
   trap** (§"Pitfall — list_view consumes UP/DOWN edges"); your
   cycler widget will hit it on day one if you don't apply the
   pattern.

---

## Goals

- Ship `pksave-gba.gba` that boots in mGBA with a symlinked Gen III
  `.sav`, detects the variant, and round-trips edits through Trainer,
  Party, Bag, and Boxes for RS / Emerald / FRLG.
- Establish the patterns (scene structure, D-pad grammar, FLASH↔EWRAM
  plumbing, widget reuse) that the NDS front-end will inherit.
- Surface library gaps as discrete checkpoint-aligned tasks against
  `libpksave`, rather than smearing them across the UI work.

## Non-goals (v1)

Out-of-scope items are enumerated in `docs/frontend-gba-libpksave-gaps.md`
("Out of v1 scope"). Notably: item descriptions, type lookups, ability
descriptions, box renaming. The scoping principle is "editor for players
who already know the game" — we surface what's needed for valid edits,
not game flavor.

---

## Locked architectural decisions

These are the load-bearing decisions from planning. Treat them as
contracts; don't relitigate during implementation.

### Build & save plumbing

1. **Cartridge save type:** declared as FLASH 128KB in the butano config
   so the editor ROM's SRAM slot matches what the Pokemon games use.
2. **SRAM access model:** at boot, copy both 64 KB FLASH banks into a
   `static uint8_t save_buf[131072]` in EWRAM. `pksave_open` is called
   on `save_buf`. All edits mutate EWRAM. `pksave_flush` finalizes
   checksums in EWRAM; FLASH is rewritten only by the explicit save
   action below.
3. **Save semantics:** lazy, user-triggered, UI-blocking. No timer, no
   auto-save. Exiting the Save & Exit menu entry runs `pksave_flush`
   followed by the FLASH bank rewrite, behind a "SAVING… DON'T TURN OFF
   THE POWER!" screen that ignores all input until the write completes.
   A dirty indicator (`●` next to the header title) appears whenever
   `save_buf` differs from FLASH.

### Scene & navigation grammar

4. **Scene pattern:** butano-idiomatic. A `scene` base class with
   `std::optional<scene_kind> update()`. Main loop holds a
   `std::unique_ptr<scene>` and swaps when `update()` returns a new
   kind. Each scene owns its own sprites/BGs; transitions clean up VRAM
   automatically.
5. **D-pad grammar** (single-purpose, no overloads):
   - **L / R shoulders** — change top-level page from main menu only.
   - **D-pad up / down** — navigate between fields, or scroll a list.
   - **D-pad left / right** — when no field is in active-edit mode:
     cycle the second-tier tabs (bag pockets, Pokemon editor sub-tabs).
     When a field is in active-edit mode: cycle the focused field's
     value.
   - **A** — enter active-edit mode on the focused field; commit inside
     an edit scene.
   - **B** — cancel / back.
   - **SELECT** — cycle character tabs inside the text-input keyboard;
     cycle boxes in the Box screen.
6. **Save & Exit is reachable only via the main menu list.** L/R never
   lands on Save. A deliberate menu action only.

### UI shapes

7. **Text input:** dedicated `text_input_scene` with a Gen 3-style 5×6
   character grid. SELECT cycles uppercase / lowercase / symbol tabs.
   Symbol set matches Gen 3 charset exactly (libpksave's encoder is the
   source of truth). Reused by trainer name, Pokemon nickname, OT name.
8. **Bag layout (Option 3):** pocket tabs at top (D-pad left/right
   cycles), slot list in left panel, A drills into a dedicated
   `slot_edit_scene` (item cycler + qty field). The item cycler is
   scoped to the current pocket via `pksave_item_pocket`. Qty cap
   per-item via `pksave_item_max_stack` — HMs and key items lock qty to
   1; TMs stack in Gen III (this is a per-item fact, not per-pocket).
9. **Pokemon editor:** three sub-tabs (IDENTITY / STATS / MOVES) with
   D-pad left/right cycling tabs. Right-panel anchor across all tabs:
   placeholder silhouette + species name + level. STATS tab shows
   calculated stats derived live via `pksave_recalc_stats`. Pokerus
   surfaced on Identity tab as a sub-modal. Shared between Party and
   Box entry points (one scene, two callers).
10. **Box screen:** linear slot list reusing the Party `list_view`
    widget. Header shows current box index; D-pad left/right at column
    boundaries (or SELECT) cycles boxes. No grid widget. Drills into the
    same `pokemon_edit_scene` as Party.
11. **Error UX:** single `boot_error_scene` displaying a halt message
    when `pksave_detect` returns UNKNOWN or `pksave_open` returns
    `PKSAVE_ERR_CORRUPT`. No interactive recovery; user fixes the
    symlink and power-cycles.

### Asset strategy

12. **Sprites — Option C:** repository ships one generic silhouette
    (drawn by us, MIT). `make gba` builds with silhouettes only by
    default. An optional extractor script accepts a user-supplied
    Pokemon Gen III ROM and dumps real 64×64 sprites into the build's
    asset directory. The editor's shipped artefact (`bin/pksave-gba.gba`)
    contains only the silhouette + the user-supplied sprites if they
    chose to extract — repo never contains Game Freak art.

### Testing strategy

13. **Two-track testing.**
    - **Maintainer track — Unity + mGBA Lua automation (planned).**
      libpksave correctness is locked in by the existing Unity suite.
      Frontend correctness (scene loop, FLASH↔EWRAM, save flow, dirty
      indicator, UI grammar) is *currently unverified* and will be
      covered by a headless-mGBA Lua harness — see §"mGBA Lua
      automation" below.  Manual mGBA runs are **not** a merge gate;
      the maintainer's time budget doesn't sustain them.
    - **User-facing track — manual checklist + fixtures.** The
      per-checkpoint checklist in `frontend-gba/tests/checklist.md` is
      published for end users who want to verify their own build, and
      as a wishlist of scenarios the Lua harness should eventually
      cover. The header of that file says so explicitly so nobody
      mistakes it for an internal QA process.

---

## File layout (target)

```
frontend-gba/
├── Makefile                          ← delegates into docker/gba
├── src/
│   ├── main.cpp                      ← boot, detect, FLASH→EWRAM, scene loop
│   ├── pksave_bridge.{hpp,cpp}       ← extern "C" wrapper + dirty tracking
│   ├── flash_io.{hpp,cpp}            ← FLASH bank-switch read/write
│   ├── scenes/
│   │   ├── scene.hpp                 ← base class + scene_kind enum
│   │   ├── boot_error_scene.{hpp,cpp}
│   │   ├── main_menu_scene.{hpp,cpp}
│   │   ├── trainer_scene.{hpp,cpp}
│   │   ├── party_scene.{hpp,cpp}
│   │   ├── box_scene.{hpp,cpp}
│   │   ├── pokemon_edit_scene.{hpp,cpp}
│   │   ├── bag_scene.{hpp,cpp}
│   │   ├── slot_edit_scene.{hpp,cpp}
│   │   └── save_scene.{hpp,cpp}      ← "DON'T TURN OFF THE POWER!"
│   └── widgets/
│       ├── list_view.{hpp,cpp}       ← scrollable list (Party, Box, Bag)
│       ├── numeric_field.{hpp,cpp}   ← hold-to-accel ±1, D-pad L/R ±10
│       ├── cycler.{hpp,cpp}          ← species/move/item/ability cycler
│       └── text_input.{hpp,cpp}      ← Pokemon-grid keyboard scene
├── graphics/
│   ├── silhouette.bmp                ← generic 64×64 placeholder (MIT, ours)
│   ├── silhouette.json               ← butano sidecar declaring it a sprite
│   └── sprites/                      ← extracted species_NNN.bmp (gitignored;
│                                       populated by tools/extract_sprites.py)
└── tests/
    ├── checklist.md                  ← manual verification per checkpoint
    └── fixtures/
        ├── ruby.sav
        ├── emerald.sav
        └── firered.sav

src/
├── species_sprite_lookup.hpp         ← lookup interface (hand-written)
└── species_sprite_lookup.cpp         ← lookup body (generated, gitignored —
                                        tools/gen_sprite_lookup.py emits it)

tools/
├── extract_sprites.py                ← optional, takes a user ROM path
└── gen_sprite_lookup.py              ← emits species_sprite_lookup.cpp from
                                        whatever BMPs landed in graphics/sprites/
```

---

## Sprint 0 — pre-UI library work — **shipped 2026-06-04**

Land before any `frontend-gba` C++ code. See gaps matrix D1, D2, D3.

- [x] **D1** — `pksave_ev_max_per_stat(game)`, `pksave_ev_total_max(game)`.
- [x] **D2** — `pksave_iv_max(game)`.
- [x] **D3** — `PKSAVE_GEN3_TRAINER_NAME_MAX`, `PKSAVE_GEN3_NICKNAME_MAX`,
      `PKSAVE_GEN3_OT_NAME_MAX` public `#define`s.
- [x] Gaps matrix updated; `make test` green.

Resolution notes live in `docs/frontend-gba-libpksave-gaps.md` §"Pre-UI
library sprint".

---

## Checkpoint #1 — boot, detect, error, main menu shell — **shipped**

First mergeable PR. Proves the FLASH↔EWRAM plumbing, the scene loop, and
the docker/gba build pipeline before any feature work.

- [x] `frontend-gba/Makefile`, butano config (FLASH 128KB),
      `flash_io.{hpp,cpp}`, `pksave_bridge.{hpp,cpp}`,
      `scenes/scene.hpp`, `boot_error_scene`, `main_menu_scene`,
      `main.cpp`, synthetic RS / Emerald / FRLG fixtures.

Done. Harness coverage: `test_editor_rom_loads.py`,
`test_flash_detected.py`, `test_fixture_roundtrip.py` (3 variants),
`test_boot_rejects_bad_save.py`, `test_main_menu_wrap.py`.

---

## Checkpoint #2 — Trainer screen + Save & Exit + text input

Closes the first real edit loop end-to-end: type a name, save, verify in
the original game.

- [x] `widgets/list_view.{hpp,cpp}` — scrollable list, generic over
      row-render callback. Used here for the Trainer field list.
- [x] `widgets/numeric_field.{hpp,cpp}` — hold-to-accelerate ±1 with
      D-pad up/down; D-pad left/right ±10; respects min/max passed in.
- [x] `widgets/text_input.{hpp,cpp}` — Pokemon-grid keyboard, SELECT
      cycles charset tabs, START commits, B deletes. Takes
      `(initial, max_len)` and returns the entered string. Charset
      widened to the full Gen 3 keyboard alphabet during L10.
- [x] `scenes/trainer_scene.{hpp,cpp}` — fields: name (drills into
      `text_input`), TID (numeric), SID (numeric), money (numeric),
      badges 1–8 (8 toggles labeled `Badge 1`–`Badge 8` per the scoping
      principle).
- [x] `scenes/save_scene.{hpp,cpp}` — blocking "SAVING… DON'T TURN OFF
      THE POWER!" screen; calls `pksave_flush` then `flash_write_all`;
      returns to main menu when done.
- [x] Wire Save & Exit in main menu to `save_scene`.
- [x] Manual checklist entries: trainer name round-trip, TID round-trip,
      money round-trip, save-blocks-input check. (Scaffolded in
      `frontend-gba/tests/checklist.md` §CP#2; emulator-side verification
      is the merge gate — see below.)

**Code-complete** as of 2026-06-05 (commit 13beb9e). Frontend behaviour
(scene loop, FLASH↔EWRAM plumbing, save-blocks-input, dirty indicator,
trainer-name round-trip on a real cartridge) is **unverified at the
emulator level** — the manual checklist in `frontend-gba/tests/
checklist.md` is published for end users / contributors who want to
spot-check, not as a maintainer merge gate.  Library-side correctness
(charset round-trip, FLASH bytes match pret canonical, stat recalc) is
covered by Unity.  Real emulator verification arrives with the mGBA Lua
harness (see §"mGBA Lua automation" below).

Bundled into the CP#2 PR: the `.gitignore` fix + `git rm --cached` of
the nine `libpksave/tests/test_*` binaries (clear miss surfaced while
closing L1/L2 — clean to ship in the same PR).

**Done when:** edit the trainer's name in fixture, choose Save & Exit,
power-cycle the emulator, reload original Pokemon ROM → trainer name
matches.

---

## Checkpoint #3 — Party + Pokemon editor (biggest checkpoint)

**Library kickoff — all four landed 2026-06-04 / 2026-06-05.** Detail:
`docs/frontend-gba-libpksave-gaps.md`.

- [x] **L1** — `pksave_recalc_stats(pkm, game)` + 25-nature mod table +
      413-entry base-stats table sourced from pret/pokeruby. Public in
      `pksave.h`. Tests in `libpksave/tests/test_gen3_recalc.c`.
- [x] **L2** — PID-recompute for `pksave_set_party_slot` /
      `set_box_slot`; ability writable via `mon->ability` (bundled).
- [x] **L4** — `pksave_species_abilities`. `gen3_base_stats_t` widened
      with `ability1` / `ability2` columns from pret.
- [x] **L5** — `pksave_move_base_pp` with 355-entry table from pret.

**Frontend work** — Pokemon editor + Party screen.

> When you ship each new scene/widget, also ship its `make test-gba`
> companion. Use `frontend-gba/tests/automation/helpers.lua` for the
> tap / dump primitives; pattern after `test_trainer_name_roundtrip.py`
> for an edit-then-FLASH-roundtrip test or
> `test_trainer_scene_nav.py` for a navigation no-mutation test.

- [x] `widgets/cycler.{hpp,cpp}` — generic name-cycler; takes
      `(current_id, max_id, lookup_fn, filter_pred)` and renders
      `◀ name ▶`. Used for species, move, item, ability, nature.
- [x] `scenes/party_scene.{hpp,cpp}` — list of 6 slots via `list_view`,
      shows species name + level + nickname per row; A drills into
      `pokemon_edit_scene` with a return-scene hint of party.
- [x] `scenes/pokemon_edit_scene.{hpp,cpp}` — three tabs, D-pad
      left/right cycles tabs when no field active-editing:
      - **IDENTITY:** species cycler, nickname (text_input), OT name
        (text_input), held item (cycler), ability (cycler, restricted
        via L4), nature (cycler), shiny toggle (functional if L2,
        display-only otherwise), Pokerus sub-modal.
      - **STATS:** level (numeric), EVs ×6 (numeric, per-stat cap from
        D1, total cap from D1), IVs ×6 (numeric, cap from D2), live
        calculated stats from L1.
      - **MOVES:** four slots, each (move cycler + PP numeric capped via
        L5 ×1.6).
- [x] Right-panel anchor across tabs: placeholder silhouette + species
      name + level. Same component on Identity / Stats / Moves.
      Silhouette path landed CP#6; per-species sprite swap is gated on
      the `make gba SPRITES_ROM=...` extraction.
- [x] Pokerus sub-modal — small popup over Identity tab, lets user pick
      NEVER / ACTIVE(strain, days) / CURED(strain). Uses libpksave's
      existing Pokerus API.

**Done when:** edit a party slot's species/moves/EVs/IVs in fixture,
save, reload in original game → Pokemon matches and stats are correct
post-heal.

---

## Checkpoint #4 — Bag — **shipped 2026-06-07**

**Library kickoff** — L6, L7 closed.

- [x] **L6** — `pksave_item_max_stack(game, item_id)` (`libpksave/src/lookups.c`
      + `names/gen3_item_max_stack.h`, 377 entries derived from
      pret/pokeemerald `.pocket` + `.importance`).
- [x] **L7** — `pksave_item_pocket(game, item_id)` (`libpksave/src/lookups.c`
      + `names/gen3_item_pocket.h`, 377 entries from pret/pokeemerald
      `.pocket`; the existing `pksave_pocket_t` enum was extended with a
      `PKSAVE_POCKET_NONE = 0xFF` out-of-band sentinel).

**Frontend work.**

- [x] `scenes/bag_scene.{hpp,cpp}` — pocket-tabbed slot list. D-pad LEFT/
      RIGHT cycle pocket (saturating); list_view drives slot navigation;
      A drills into `slot_edit_scene` with the pocket+slot recorded on
      the bridge; B returns to main menu. Header shows `BAG <POCKET>`
      with the dirty `*` indicator. Empty slot rows render `[Empty]`.
- [x] `scenes/slot_edit_scene.{hpp,cpp}` — two-row editor (ITEM cycler
      + QTY numeric_field). Item cycler filter is scoped via
      `pksave_item_pocket` (L7) — plus item_id 0 is always reachable as
      a clear sentinel. Qty cap is reseated from `pksave_item_max_stack`
      (L6) on every item change. A commits via `pksave_set_item` and
      marks the bridge dirty; B cancels. UP/DOWN navigate the two rows
      (list_view); LEFT/RIGHT are the qty edit axis on ROW_QTY (±10);
      cycler step axis on ROW_ITEM. The same list_view-edge-consumption
      pitfall as trainer_scene applies — both fields disarm on focus.
- [x] Empty-slot add path: drilling on a row past `item_count_` opens
      the editor with item_id = 0 / qty = 0, so cycling to a real item
      and committing adds the slot.
- [x] Delete path: committing with item_id = 0 forces qty = 0, which
      `pksave_set_item` handles as a tombstone.
- [x] Per-game pocket-count differences (RSE 46 berries, FRLG 43, etc.)
      handled by trusting `pksave_get_pocket`'s returned count — the
      same pattern as the seed buffer in `bag_scene::load_pocket`.
- [x] mGBA-Lua harness coverage:
      - `test_bag_scene_nav.py` (main_menu → Bag → cycle pockets → B →
        main_menu; verifies trainer name + items[0] preserved through
        the round-trip).
      - `test_bag_slot_edit_roundtrip.py` (drives Items[0].qty 5 → 15
        through FLASH on RS fixture; trainer name + adjacent slot
        preserved as negative controls).

**Done when:** edit Items slot 0 qty in the rs fixture, save & exit,
reload → quantity matches what was set. *(Met by
`test_bag_slot_edit_roundtrip.py`.)*

**Follow-ups (deferred):**

- 3-variant fanout for the bag round-trip (currently RS-only;
  Emerald + FRLG would mirror the L1 pattern).
- "Swap item between pockets" path — currently the user can only edit
  in-pocket; cross-pocket swaps require a Delete-then-Add pair.
- Per-checkpoint manual-checklist row in `frontend-gba/tests/checklist.md`.

---

## Checkpoint #5 — Boxes — **shipped 2026-06-07**

One library gap discovered + closed (L11 — box-mon level/XP decode +
encode); L1–L7 covered everything else for `pokemon_edit_scene` reuse.

**Library work.**

- [x] **L11** — XP↔level math in `libpksave/src/gen3.c` (formulas from
      pret/pokeemerald `src/data/pokemon/experience_tables.h`, runtime
      computation), plus a 413-entry per-species growth-rate lookup
      in `libpksave/src/names/gen3_growth_rates.h` sourced from
      pret/pokeemerald `src/data/pokemon/species_info.h`.
      `g3_decode_box_mon` now derives `out->level` from the 4-byte XP
      at the Growth substruct's offset 4; `g3_encode_encrypted_block`
      writes XP from `mon->level` using the new species's growth-rate
      curve, preserving existing XP exactly when both species and
      level are unchanged (no-op edits don't lose within-level XP
      progress).  Fixture generator updated to seed XP at the
      canonical threshold rather than `level * 10`.

**Frontend work.**

- [x] `pksave_bridge` extended with `pending_box_index` /
      `pending_box_slot` (defaults -1) and `set_pending_box` /
      `clear_pending_box` — mirroring the bag handoff pattern.
- [x] `scenes/box_scene.{hpp,cpp}` — linear 30-slot `list_view`.
      Header renders `BOX N  filled/30` plus the dirty `*`.  SELECT
      cycles boxes (wraps via `% box_total_`; `pksave_get_box_count`
      returns 14 for Gen 3).  A records `(box_index_, list_.selected())`
      on the bridge and returns `scene_kind::POKEMON_EDIT`.  B returns
      to MAIN_MENU.  `clear_pending_box()` on ctor makes re-entry
      idempotent (same pattern as `bag_scene`).
- [x] `pokemon_edit_scene` branches on a new `edit_source` enum
      (PARTY vs BOX), decided in the ctor by checking
      `pending_box_index() >= 0` first then falling back to
      `pending_party_slot()`.  Branches `load_from_save` (party vs box
      get), `push_to_save` (party vs box set), and B-return (PARTY vs
      BOXES).  ROW_POKERUS guarded as no-op when source==BOX (see L11).
- [x] `main.cpp` and `main_menu_scene` wire `scene_kind::BOXES` to
      `box_scene`.  Previously the BOXES menu entry fell back to
      MAIN_MENU.
- [x] mGBA-Lua harness coverage:
      - `test_box_scene_nav.py` (main_menu → Boxes → SELECT×3 → B →
        main_menu; verifies trainer name + box[1][0] preserved through
        the round-trip — no spurious FLASH writes).
      - `test_box_species_roundtrip.py` (drives box[1][0] Bulbasaur →
        Ivysaur through FLASH on the RS fixture; trainer name, box[0]
        empty, party[0] unchanged all serve as negative controls
        against off-by-one box-index, wrong-source push, and
        whole-save corruption).
      - `test_box_stats_level_roundtrip.py` (drives box[1][0] level
        5 → 35 through FLASH; proves L11 plumbing end-to-end from the
        STATS-tab numeric_field through `pksave_set_box_slot`'s XP
        write back to `pksave_get_box`'s XP-derived level decode).

**Done when:** edit a box-resident Pokemon's species, save, reload →
Pokemon matches. *(Met by `test_box_species_roundtrip.py`.)*

**Follow-ups (deferred):**

- **Pokerus from box source** — `pokerus_modal_scene` keys on
  `pending_party_slot` only; entering it from a box drill would point
  at the wrong slot.  The ROW_POKERUS branch in `pokemon_edit_scene`
  is therefore guarded as a no-op when source==BOX.  Extending
  `pokerus_modal_scene` to take `(source, box, slot)` and branching to
  `pksave_{get,set}_box_pokerus` (both already in `pksave.h`) is the
  follow-up; adds ~30 LOC + 2 harness tests.
- 3-variant fanout for box species round-trip (currently RS-only;
  Emerald + FRLG would mirror the L1 trainer-name fanout pattern).
- Cross-box "move slot" path — currently only in-slot species/etc
  edits; moving a mon between boxes requires Delete-then-Add.

---

## Checkpoint #6 — Sprites (Option C) — **closed 2026-06-10**

> Format pivot: butano 18.5's grit only consumes BMP (not PNG); the
> plan's original `silhouette.png` / extracted-`.png` references are
> now 4bpp indexed BMP.  All else as planned.

- [x] `frontend-gba/graphics/silhouette.bmp` — generic 64×64 silhouette,
      drawn by us (`tools/make_silhouette.py` is a one-shot regenerator;
      output is committed).
- [x] butano asset import for the silhouette (sidecar
      `silhouette.json`); rendered on the pokemon editor IDENTITY/MOVES
      right panel via `bn::sprite_items::silhouette`.  STATS tab drops
      the silhouette and keeps the existing stat list.  Party/Box list
      scenes do **not** have right panels in the current layout; if
      that changes, the silhouette would slot in via the same pattern.
- [x] `tools/extract_sprites.py` — accepts `--rom /path/to/gen3.gba`,
      dumps one 64×64 indexed BMP + JSON sidecar per species into
      `frontend-gba/graphics/sprites/`.  No-op when the directory is
      already populated.  29 unit tests in `tools/test_extract_sprites.py`
      (`make test-tools`).
- [x] Makefile target: `make gba SPRITES_ROM=/path/to/rom.gba` runs the
      extractor before invoking the butano build (root `Makefile`
      `_maybe_extract_sprites`).  `GRAPHICS` in `frontend-gba/Makefile`
      includes `graphics/sprites` so grit picks up the extracted BMPs
      when present.
- [x] README section documenting the extractor's existence, legal
      caveat, and how to use it (top-level `README.md` →
      "Optional: extract real Pokémon sprites").
- [x] Pokemon editor right panel: if extracted sprite exists for the
      current species, render it; else render the silhouette.
      `tools/gen_sprite_lookup.py` scans `frontend-gba/graphics/sprites/`
      and emits `frontend-gba/src/species_sprite_lookup.cpp`, a
      `pksave_gba::sprite_lookup::species_sprite_for(int)` switch
      mapping each present species id to its
      `bn::sprite_item* species_NNN`.  `pokemon_edit_scene::redraw_right_panel`
      calls the lookup on every redraw and falls back to
      `bn::sprite_items::silhouette` on nullptr.  Generated file is
      gitignored; the root `Makefile` runs the generator after the
      optional `_maybe_extract_sprites` step, so fresh clones get a
      no-op switch (silhouette everywhere) and `make gba SPRITES_ROM=...`
      builds get the per-species art.  7 unit tests in
      `tools/test_gen_sprite_lookup.py` (`make test-tools`).

**Done when:** `make gba` with no args still ships a working ROM (with
silhouettes); `make gba SPRITES_ROM=...` produces a ROM where the
Pokemon editor right panel shows the real sprite for the current
species.  *Status: both clauses met.*

**Auto-discovery (2026-06-09 — closes `_VARIANT_TABLES` follow-up):**

`tools/extract_sprites.py` locates `gMonFrontPicTable` and
`gMonPaletteTable` via structural signature rather than hardcoded
offsets.  Pret initializes both arrays with designated initializers
`[SPECIES_X] = { ..., SPECIES_X }`, so for every entry `i` the tag
field equals the array index.  The scanner walks 4-byte-aligned
offsets and accepts the first run of ≥50 entries matching:

- pic table:  `(ptr ∈ 0x08xxxxxx, size == 0x800, tag == i)`
- pal table:  `(ptr ∈ 0x08xxxxxx, tag == i, _pad == 0)`

Forward-extends from the threshold to discover the actual species
count.  Region-agnostic — works on US/JP/EU/revision variants
without hardcoded addresses.  6 new unit tests in
`tools/test_extract_sprites.py` cover scanner correctness, no-match
rejection, front-vs-back-table disambiguation (first-occurrence
wins), and end-to-end extract_all via discovery.  Total
`make test-tools` count: 35.

**Per-species render branch (shipped 2026-06-10):**

`tools/gen_sprite_lookup.py` generates `frontend-gba/src/species_sprite_lookup.cpp`
on every `make gba`, scanning `frontend-gba/graphics/sprites/` for
`species_NNN.bmp` filenames and emitting one `case N: return
&bn::sprite_items::species_NNN;` per present id.  The corresponding
hand-written header `frontend-gba/src/species_sprite_lookup.hpp`
declares `pksave_gba::sprite_lookup::species_sprite_for(int)`;
`pokemon_edit_scene::redraw_right_panel` calls it on every redraw and
substitutes `bn::sprite_items::silhouette` when the lookup returns
nullptr (no BMP extracted for that species).  Generator is deterministic
(stable sort by id, fixed boilerplate) so incremental rebuilds stay
diff-clean.  7 host-side tests in `tools/test_gen_sprite_lookup.py`
cover empty/missing dirs, single + multiple species, decoy rejection
(silhouette.bmp / .json sidecars / wrong extensions), deterministic
output, and signature/namespace contract.

The generated `.cpp` lives at `frontend-gba/src/species_sprite_lookup.cpp`
and is gitignored — fresh clones materialize a no-op switch on first
`make gba`.  The grit-emitted `bn::sprite_items::species_NNN` symbols
are link-time, which is why we have to gate on the BMPs being present
on disk *before* the butano build runs.

**Follow-ups (deferred):**

- **Party/Box right-panel** — those scenes are list-only today; if a
  right-panel column is added later, port the same lookup pattern from
  `pokemon_edit_scene::redraw_right_panel`.
- **3-variant fanout** — extractor + silhouette are RS/EMERALD/FRLG
  agnostic; no real-ROM end-to-end test exists yet because the repo
  doesn't ship Gen 3 ROMs (legal).  A maintainer with a personal
  copy can run `make gba SPRITES_ROM=...` and inspect the output, but
  this stays off the merge gate.

---

## mGBA Lua automation — **shipped 2026-06-06 / 07**

The only real frontend verification layer. `make test-gba` is a hard
merge gate. See `docs/frontend-gba-harness-plan.md` for the full
reference (Dockerfile, mgba-lua.c quirks, helpers.lua API, per-test
descriptions).

### What's in the harness today

- **`docker/test-gba` container** — headless custom `mgba-lua` binary
  embedding libmgba + the Lua engine (upstream SDL frontend doesn't
  ship Lua bindings; we link them in via our own short C wrapper).
  Pinned to `MGBA_TAG=0.10.5`.
- **`mgba-lua` CLI** — `mgba-lua <rom> <script.lua> [frames] [--save <path>]`.
  Sidecar save load uses `mCoreLoadSaveFile(core, savePath, true)`
  (temporary mode; writes don't persist).
- **Python harness** in `frontend-gba/tests/automation/` — each test
  is a plain Python 3 script (no pytest) that subprocesses into
  `docker run`, mounts a save and dump dir, and asserts on the SRAM
  dump and on `console:log` markers from the Lua side.
- **`libpksave.so`** — built by `make -C libpksave lib`; loaded via
  ctypes wrapper at `frontend-gba/tests/automation/pksave_ctypes.py`.
  Exposes `detect`, `open`, `get_trainer_name`, `get_trainer`, `close`.
- **`helpers.lua`** — `M.at(frame, fn)`, `M.tap(key, frame, hold=3)`,
  `M.sram_dump(path)`, `M.KEY.{A,B,SELECT,START,UP,DOWN,LEFT,RIGHT,L,R}`.
  When butano debounce or mGBA frame timing changes, tune helpers, not
  N test scripts.

### Test inventory (30 tests, all green)

| Test                              | What it proves |
|-----------------------------------|----------------|
| `test_decode_fixture.py`          | ctypes decodes RS / Emerald / FRLG fixtures |
| `test_mgba_harness_smoke.py`      | Python → docker → mgba-lua → stdout |
| `test_editor_rom_loads.py`        | pksave-gba.gba loads as a GBA cartridge |
| `test_sram_dump.py`               | 16-byte tracer through Lua → file → Python |
| `test_sram_dump_full.py`          | full SRAM/FLASH region via mirror-probe |
| `test_flash_detected.py`          | mGBA logs `Detected Flash savegame` after boot (FLASH 128 KiB) |
| `test_key_input.py`               | A-tap flips KEYINPUT bit 0 (`0x03ff → 0x03fe`) |
| `test_sidecar_save.py`            | `--save rs.sav` puts the fixture's first 16 bytes into SRAM |
| `test_sram_dump_full.py`          | mirror-probe full dump |
| `test_fixture_roundtrip.py`       | 3-variant boot-load → dump → decode (RS/Emerald/FRLG) |
| `test_trainer_scene_nav.py`       | navigation (`A` from main menu) produces zero FLASH writes |
| `test_trainer_name_roundtrip.py`  | 3-variant trainer-name edit RED → REDAAA through FLASH |
| `test_boot_rejects_bad_save.py`   | malformed fixture → boot_error halt; no FLASH writes |
| `test_tid_roundtrip.py`           | TID +30 through numeric_field; name preserved |
| `test_main_menu_wrap.py`          | DOWN×5 wraps cursor back to Trainer |
| `test_dirty_indicator.py`         | header strip CLEAN → DIRTY → AFTER_SAVE hash cycle via fb_dump_ppm |

### Coverage gaps (CP#3+ to-add as scenes land)

- [x] Party-slot species/level/move round-trip (CP#3 — shipped)
- [x] Bag item/qty round-trip (CP#4 — `test_bag_slot_edit_roundtrip.py`,
      RS-only; 3-variant fanout still open)
- [x] Box-slot species round-trip (CP#5 — `test_box_species_roundtrip.py`,
      RS-only; 3-variant fanout still open)
- [x] Dirty-indicator visual — `test_dirty_indicator.py` snapshots the
      header strip before edit, after edit (TID +10 via numeric_field
      → mark_dirty), and after Save & Exit. Asserts hash flip on edit,
      hash restoration on save. Backed by `fb_dump_ppm` (custom Lua
      global in `docker/test-gba/mgba-lua.c`) + `framebuffer.py`.
- [ ] Save-blocks-input check — currently inferred from Gate J's clean
      completion, not directly asserted

### Pattern for adding a new test

1. **One Lua drive script** in
   `frontend-gba/tests/automation/drive_<feature>.lua`. Use
   `dofile("/src/frontend-gba/tests/automation/helpers.lua")` at the
   top. Schedule input with `h.tap(h.KEY.<X>, <frame>)`. Dump on
   `shutdown` via `h.sram_dump("/dump/sram.bin")`. Generous inter-tap
   gaps (~30 frames). Budget ~480 total frames for a "boot → drive →
   save → dump" sequence.
2. **One Python test** at
   `frontend-gba/tests/automation/test_<feature>.py`. Mirror
   `test_trainer_name_roundtrip.py` for "load fixture, drive, dump,
   decode, assert" or `test_trainer_scene_nav.py` for
   "no-fixture, drive, assert FLASH untouched".
3. **One line in the root `Makefile`** appending to `GBA_TESTS`.
4. Run from repo root: `python3 frontend-gba/tests/automation/test_<feature>.py`.
   When green, run `make test-gba` to confirm no other tests broke.

### Relationship to the manual checklist

`frontend-gba/tests/checklist.md` stays as the user-facing verification
doc and a wishlist of scenarios the harness should cover. Each new Lua
test should reference the checklist row it discharges so the two stay
synchronised.

---

## Acceptance criteria (v1 complete)

The maintainer doesn't run manual mGBA checks per merge, so criteria
that rely on a human driving the emulator have been recast as
machine-checkable equivalents (Unity for byte-level correctness; the
mGBA Lua harness for scene-loop and save-flow).  The user-facing
manual checklist in `frontend-gba/tests/checklist.md` stays in the
repo but is not on this list.

- [x] `make gba` (no args) produces `pksave-gba.gba` — ships with the
      generic silhouette on the pokemon-editor right panel; the
      `species_sprite_lookup` switch is a no-op (returns nullptr for
      every species, so every species renders the silhouette).
- [x] `make gba SPRITES_ROM=...` produces a ROM where the pokemon-editor
      right panel shows the real per-species sprite, falling back to
      the silhouette for any species absent from the ROM dump.
- [x] `make test` green — Unity coverage across all Gen flags (19
      new L6/L7 cases on top of the existing suite).
- [ ] `make test-gba` green — mGBA Lua harness covers:
      - [x] boot + variant detection on RS / Emerald / FRLG fixtures
      - [x] UNKNOWN-content fixture lands on the boot error scene
      - SRAM-roundtrip after Save & Exit for at least one field per CP:
        - [x] trainer name (Gate J × 3 variants)
        - [x] trainer TID (numeric_field exercise)
        - [x] party-slot species *(CP#3)*
        - [x] bag item quantity *(CP#4 — RS only;
              `test_bag_slot_edit_roundtrip.py`)*
        - [x] box-slot species *(CP#5 — RS only;
              `test_box_species_roundtrip.py`)*
      - [x] dirty indicator appears after an edit and clears after save
        *(`test_dirty_indicator.py` — fb_dump_ppm + header-strip hash;
        CLEAN/DIRTY/AFTER_SAVE snapshots assert that '*' is added on
        edit and removed on Save & Exit, with the AFTER_SAVE hash byte-
        equal to CLEAN)*
      - [ ] save scene blocks input while the FLASH write is in flight
        *(currently inferred from Gate J's clean completion)*
- [x] `docs/frontend-gba-libpksave-gaps.md` reflects which gaps were
      filled, which were deferred, and why.

**Out-of-scope for v1 acceptance** (filed for follow-up automation /
end-user QA):
- Loading the saved `.sav` back into an unmodified Pokémon ROM and
  verifying the in-game trainer card / party menu renders the edit.
  Requires running a second ROM under mGBA against the same FLASH
  image — possible under the Lua harness but not in v1 scope.
- Visual / palette / sprite-tearing regressions.  Out of reach for
  SRAM-only assertions; future framebuffer-byte checks may cover the
  load-bearing cases (Save screen visible, sprite present-or-silhouette
  fallback).

---

## Deferred follow-ups (not blockers for the NDS frontend)

These were surfaced during CP#3–CP#6 hardening and the framebuffer
harness work (2026-06-10). None of them blocks starting CP#7 (NDS
frontend); they're parked here so a future cleanup session (or a
maintainer who has a personal Gen 3 ROM to test with) can pick one
without re-deriving the context.

### Gen 4 / Gen 5 ability name tables are still stubs

`libpksave/src/names/gen4_abilities.h` and `gen5_abilities.h` contain
only `"???"` at index 0 — same shape Gen 3 had before 2026-06-10. The
Gen 3 stub was a real UX bug (the editor's ability cycler rendered
every label as `???`); for Gen 4/5 it's latent because no frontend
exposes the cycler yet. `docs/coverage.md` now reads `❌ stub` for
those cells so it's not a silent lie. **Resolve before the NDS
frontend's pokemon-edit equivalent reaches the ability row** — the
fix shape is exactly the Gen 3 patch: `WebFetch` the table from pret
(`pret/pokeplatinum` for Gen 4, `pret/pokeblack`/`pret/pokeblack2` for
Gen 5), title-case to match the existing convention, add a handful of
spot-check Unity cases to `test_gen4_lookups.c` / `test_gen5_lookups.c`
(file the tests if they don't exist; pattern after
`test_gen3_lookups.c`'s ability-name block).

### Save-blocks-input has no direct framebuffer assertion

`docs/frontend-gba-plan.md` §"Acceptance criteria" still has *save
scene blocks input while the FLASH write is in flight* as inferred
("currently inferred from Gate J's clean completion"). Now that
`fb_dump_ppm` exists, this closes the same way the dirty indicator did:
one drive script entering Save & Exit + tapping the D-pad while the
"SAVING…" splash is on screen, with two snapshots — one mid-save
asserting the splash glyphs are visible, one after asserting the
cursor on the post-save main menu didn't move during the splash
window. ~50 LOC, mirror after `test_dirty_indicator.py`. Latent risk
is low (no observed regression) but a regression here would corrupt
saves on a power-off, so closing it is cheap insurance.

### Pokerus-from-box-source is a no-op

`pokemon_edit_scene` guards `ROW_POKERUS` as a silent no-op when
`source==BOX` (introduced CP#5; documented inline). Players will see
the row, press A, and nothing happens. The fix shape — widening
`pokerus_modal_scene` to take a `(source, box, slot)` tuple and
dispatching to `pksave_{get,set}_box_pokerus` (both already in
`pksave.h`) — is ~30 LOC + 2 harness tests modelled on
`test_pokerus_modal_commit.py`. Currently surfaces as a UX dead end,
not corruption.

### 3-variant fanout for bag / box round-trip is still RS-only

`test_bag_slot_edit_roundtrip.py` and `test_box_species_roundtrip.py`
exercise RS only. Emerald + FRLG fanout would mirror the L1 trainer-
name pattern (parametrise over the fixture name). No Gen 3 variant
divergence is known in the bag or box layouts (RS / Emerald / FRLG
agree per `libpksave/src/gen3.c`), so the existing RS coverage is the
load-bearing one; the fanout is defence in depth, not closure of an
open risk.

### Party / Box right-panel sprite swap

CP#6 wired the per-species sprite lookup through the pokemon-editor
right panel only. The party and box scenes' right columns are list-
only; if a right-panel sprite preview is added there later, port
`pokemon_edit_scene::redraw_right_panel`'s lookup pattern verbatim
(the silhouette + `species_sprite_lookup` switch already handles the
"not extracted" fallback).

### 3-variant real-ROM sprite test

The sprite extractor + silhouette path is RS/EMERALD/FRLG agnostic
in tools/, but there's no end-to-end test that runs `make gba
SPRITES_ROM=...` against a real Gen 3 dump because the repo doesn't
bundle Gen 3 ROMs (legal). A maintainer with a personal copy can
verify locally; stays off the merge gate.

---

## What this plan does not do

- Does not extend `libpksave` for Gen IV/V — those gaps are the Gen IV
  developer's discovery loop.
- Does not surface game flavor (types, item descriptions, ability
  descriptions, named badges, nature stat modifiers). See the scoping
  principle in the gaps matrix.
- Does not fill L8 / L9 (badge names, nature modifiers). Filed as known
  library gaps for future consumers.

---

## Patterns and lessons (read before writing CP#3 code)

Discoveries surfaced during CP#1, CP#2, and harness work. Each one is
small in isolation; collectively they're the difference between a clean
landing for CP#3 and a re-discovery loop.

### Pitfall — `list_view` consumes UP/DOWN edges; level-triggered field widgets must arm-on-focus

`widgets/list_view.cpp` reads `bn::keypad::up_pressed()` and
`down_pressed()` — edge-triggered, one cursor step per press edge.
`widgets/numeric_field.cpp` reads `up_held()` / `down_held()` and
maintains its own per-axis hold-frame counter — level-triggered, with a
"frame 1 always steps" rule for tap responsiveness.

Both ticks run in the same frame from `trainer_scene::update`:

```cpp
const bool selection_changed = list_.tick();    // edge-consumed UP/DOWN
const int sel = list_.selected();
if (sel == ROW_TID) { tid_.tick(); }            // level-read sees STILL-held DOWN
```

So pressing DOWN to navigate from ROW_NAME to ROW_TID **also** debits
TID by 1 (numeric_field reads down_held=true on the same frame and
treats it as a fresh "frame 1" step). Symmetric for UP.

**Fix — already applied** to `numeric_field` + `trainer_scene`:

- `numeric_field` exposes `on_focus_acquired()`. It disarms any axis
  currently held; tick() ignores disarmed axes until they're released.
  Axes re-arm automatically when the user lifts the key.
- `trainer_scene::update` calls `tid_.on_focus_acquired()` (and `sid_`,
  `money_`) whenever `list_.tick()` returns true and the destination is
  a numeric row.

**You will hit this in CP#3** when you ship `widgets/cycler`: it
probably reads L/R edges or held axes too. If it reads `*_held()` for
hold-to-fast-cycle, give it the same `on_focus_acquired()` method and
call it from `pokemon_edit_scene::update`. Pattern: anything that does
level-triggered input inside a list_view-driven scene needs disarm-on-
focus, or the navigation edge that lands focus on you also "uses" you.

Regression test: `frontend-gba/tests/automation/test_tid_roundtrip.py`.
Asserts a clean `+30` from three RIGHT taps; if a future widget change
reintroduces the bug the test reports `+29` and tells the reader why.

### Pitfall — the editor ROM is `.gitignore`-d; `make test-gba` will use a stale binary

`frontend-gba/pksave-gba.gba` is **not in git** (matches the
`.gitignore` pattern `frontend-gba/*.gba`). Each contributor has their
own local copy. If you change editor source and run `make test-gba`
without first running `make gba`, you are testing **the previous
build**. The harness has no way to detect this; a "passing" test means
"the binary you happen to have on disk passes," not "your changes
pass."

Gate J once spent ~hours chasing a "broken text_input tab=2" mystery
that turned out to be a stale ROM committed for the wrong reason. Don't
repeat it: **run `make gba && make test-gba` together** when you've
changed anything in `frontend-gba/src/`.

### Pattern — mGBA's save-type autodetect is *lazy*; wait for boot before asserting

mGBA picks SRAM vs. FLASH based on the **first cartridge-memory access**.
A read at `0x0E000000` ⇒ SRAM. A write to `0x0E005555` (the FLASH
unlock magic) ⇒ FLASH. The editor's `bn::core::init` →
`flash_read_all` writes the unlock command during early boot (frames
~15). Any harness probe that touches FLASH/SRAM at frame 0 will lock
mGBA to SRAM 32 KiB before the editor gets to fire FLASH unlock.

**In test code**: do your SRAM dump in the `shutdown` callback (after
all N frames have advanced) or wait ≥60 frames in a `frame` callback.
See `dump_after_boot.lua` for the canonical pattern.

### Pattern — mGBA Lua's `emu.memory.<region>` is lazy-registered

Per-region accessors (`emu.memory.sram`, `emu.memory.ewram`) only
populate once the region has been accessed. Lua's `emu.memory.sram` is
`nil` until something reads or writes the SRAM range. Workaround in
`helpers.lua`'s `sram_read_all`: warm-up `emu:read8(0x0E000000)` first,
then dump via byte loop. Alternative: write a marker, find the lowest
power-of-2 offset whose readback matches the marker — that's the
address-mirror wrap point and equals region size. See
`dump_sram_full.lua` for the mirror-probe pattern.

### Pattern — drive script frame budget

A typical "boot → drive UI → Save & Exit → FLASH write → dump" sequence
needs ≥ ~480 frames at the default tap pacing:

- ~30 frames for boot + main menu stabilisation
- ~30 frames per cursor / scene transition (gates need this for the
  edge-triggered keypad state to settle)
- ~50 frames for the synchronous FLASH write inside `save_scene`

Tests using `text_input` editing typically run 480 frames; the wrap
test (`test_main_menu_wrap.py`) uses 600 because it also walks the cursor
through 5+ extra DOWNs.

### Pattern — `tap` is the universal input primitive

`h.tap(KEY, frame, hold=3)` schedules a press at `frame` and a release
at `frame + hold`. 3-frame hold clears the butano debounce on
edge-triggered inputs (A, B, START, SELECT, list-view UP/DOWN) and
produces exactly one step in `numeric_field` (since
`should_step_this_frame(1)` returns true and frames 2-9 are the dead
zone). If you ever need to test the hold-to-accelerate path, use
`h.at(frame, fn)` directly to compose holds longer than the dead zone
(≥ 10 frames).

### Lesson — Track real artifacts vs. test artifacts in the fixture generator

`libpksave/tests/fixtures/generate_fixtures.py` had a slice-assignment
that silently grew Gen-3 fixtures from 131,072 to 278,520 bytes (Gen-3
sections are 4,096 bytes; the bug used `GEN3_SIZE // GEN3_NUM_SECTIONS`
which is 9,362 not 4,096). C tests `fread`-ed only 128 KiB so they
passed; the harness tried to load the whole file and failed `pksave_detect`.

The regression guard is now `libpksave/tests/test_fixtures.c` — opens
each fixture and asserts its on-disk size matches the canonical value.
**When you add a Gen IV/V fixture or change Gen 3 generation, add a row
to `test_fixtures.c`'s expected-size table.**
