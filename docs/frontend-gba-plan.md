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
- `PRD.md` / `SPEC.md` / `AGENT_PROMPT.md` — feature requirements,
  technical spec, and the original build order.

Date produced: 2026-06-03.

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
├── assets/
│   ├── silhouette.png                ← generic 64×64 placeholder (MIT, ours)
│   └── ui/                           ← fonts, frame chrome (MIT, ours)
└── tests/
    ├── checklist.md                  ← manual verification per checkpoint
    └── fixtures/
        ├── ruby.sav
        ├── emerald.sav
        └── firered.sav

tools/
└── extract_sprites.py                ← optional, takes a user ROM path
```

---

## Sprint 0 — pre-UI library work

Land before any `frontend-gba` C++ code. See gaps matrix D1, D2, D3.

- [ ] **D1** — `pksave_ev_max_per_stat(game)`, `pksave_ev_total_max(game)`
      in `pksave.h`; tables in `gen3.c` (and Gen I/II/IV/V equivalents);
      Unity tests in `tests/test_genN.c`.
- [ ] **D2** — `pksave_iv_max(game)` in `pksave.h`; tables; Unity tests.
- [ ] **D3** — `PKSAVE_GEN3_TRAINER_NAME_MAX`, `PKSAVE_GEN3_NICKNAME_MAX`,
      `PKSAVE_GEN3_OT_NAME_MAX` as public `#define`s in `pksave.h`;
      per-gen equivalents added when later frontends land.
- [ ] Update `docs/frontend-gba-libpksave-gaps.md`: mark D1–D3 resolved.
- [ ] `make test` green across all Gen flags.

**Done when:** `pksave.h` exposes the new helpers, all Unity tests pass,
the gaps matrix is updated.

---

## Checkpoint #1 — boot, detect, error, main menu shell

First mergeable PR. Proves the FLASH↔EWRAM plumbing, the scene loop, and
the docker/gba build pipeline before any feature work.

- [ ] `frontend-gba/Makefile` — invokes `docker/gba`, builds
      `bin/pksave-gba.gba`, links a static `libpksave.a` built with
      `-DPKSAVE_GEN3`.
- [ ] butano config — declare cartridge save type as FLASH 128KB.
- [ ] `flash_io.{hpp,cpp}` — `flash_read_all(uint8_t out[131072])` and
      `flash_write_all(const uint8_t in[131072])` with bank switching.
- [ ] `pksave_bridge.{hpp,cpp}` — RAII wrapper around `pksave_save_t *`;
      owns the EWRAM `save_buf`; exposes `game()`, `is_dirty()`,
      `mark_dirty()`, `flush_to_flash()`.
- [ ] `scenes/scene.hpp` — base class, `scene_kind` enum.
- [ ] `scenes/boot_error_scene.{hpp,cpp}` — halt with message.
- [ ] `scenes/main_menu_scene.{hpp,cpp}` — five entries
      (Trainer / Party / Bag / Boxes / Save & Exit), L/R cycles the
      top-level pages (no-op until other scenes exist), header shows
      detected game and the dirty `●`.
- [ ] `main.cpp` — boot, copy FLASH→EWRAM, call `pksave_detect` and
      `pksave_open`, route to `boot_error_scene` or `main_menu_scene`.
- [ ] `tests/checklist.md` initial scaffold.
- [ ] `tests/fixtures/` — one fixture each for RS, Emerald, FRLG
      (synthetic if real saves unavailable; generator script committed).

**Done when:** `make gba` produces `pksave-gba.gba`; mGBA loads it with
each fixture and the main menu shows the correct game name; loading a
random binary shows the error screen.

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

**Library kickoff** — close gaps L1, L2, L4, L5 before UI work begins.

- [ ] **L1** — `pksave_recalc_stats(pkm, game)` with Gen III base-stats
      table (387 species × 6 stats), nature multipliers, official
      formula. Unity tests against known in-game values.
- [ ] **L2** — verify `pksave_set_party_slot` round-trips `is_shiny` in
      `gen3.c`. If yes, write a regression test. If no, add explicit
      `pksave_set_pokemon_shiny` or PID-recompute support, with tests.
- [ ] **L4** — `pksave_species_abilities(game, species_id)` table; tests.
- [ ] **L5** — `pksave_move_base_pp(game, move_id)` table; tests.

**Frontend work** — Pokemon editor + Party screen.

- [ ] `widgets/cycler.{hpp,cpp}` — generic name-cycler; takes
      `(current_id, max_id, lookup_fn, filter_pred)` and renders
      `◀ name ▶`. Used for species, move, item, ability, nature.
- [ ] `scenes/party_scene.{hpp,cpp}` — list of 6 slots via `list_view`,
      shows species name + level + nickname per row; A drills into
      `pokemon_edit_scene` with a return-scene hint of party.
- [ ] `scenes/pokemon_edit_scene.{hpp,cpp}` — three tabs, D-pad
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
- [ ] Right-panel anchor across tabs: placeholder silhouette + species
      name + level. Same component on Identity / Stats / Moves.
- [ ] Pokerus sub-modal — small popup over Identity tab, lets user pick
      NEVER / ACTIVE(strain, days) / CURED(strain). Uses libpksave's
      existing Pokerus API.
- [ ] Update `tests/checklist.md` with party edit round-trip checks per
      variant (RS, Emerald, FRLG).

**Done when:** edit a party slot's species/moves/EVs/IVs in fixture,
save, reload in original game → Pokemon matches and stats are correct
post-heal.

---

## Checkpoint #4 — Bag

**Library kickoff** — close gaps L6, L7.

- [ ] **L6** — `pksave_item_max_stack(game, item_id)` table; Unity test
      covering HMs (return 1), key items (return 1), TMs (return 99 in
      Gen III), stackable items, berries (999), unknown ID (return 0).
- [ ] **L7** — `pksave_item_pocket(game, item_id)` table; Unity test per
      pocket.

**Frontend work.**

- [ ] `scenes/bag_scene.{hpp,cpp}` — pocket tabs at top (Items / Balls /
      TMs/HMs / Key / Berries), D-pad left/right cycles pockets, slot
      list via `list_view`. A on a slot drills into `slot_edit_scene`.
      Empty slots show `[Empty]`.
- [ ] `scenes/slot_edit_scene.{hpp,cpp}` — two fields: item cycler
      (scoped via L7), qty numeric field (cap from L6, dynamically
      updates when item changes; greyed/locked at 1 for unique items).
      A saves changes back to `save_buf`, B cancels.
- [ ] Empty-slot add flow: open `slot_edit_scene` with item = `(none)`
      and qty = 0; cycle to a real item with qty > 0 to add.
- [ ] Delete flow: set qty to 0 → confirm prompt → libpksave handles the
      tombstone via `pksave_set_item`.
- [ ] Handle per-game pocket count differences (Berries has 46 in RSE,
      43 in FRLG; etc.) by trusting `pksave_get_pocket`'s returned count.
- [ ] Update `tests/checklist.md` with bag edit round-trips.

**Done when:** edit a Potion qty, swap a Master Ball into the Balls
pocket, add a Berry to an empty Berries slot, delete a TM → save,
reload in original game → all changes persist, no save corruption.

---

## Checkpoint #5 — Boxes

No new library work (L1–L7 already cover what the Pokemon editor needs).

- [ ] `scenes/box_scene.{hpp,cpp}` — linear slot list of 30 slots,
      reuses Party's `list_view` exactly. Header shows `BOX N` and the
      box's filled count. A drills into `pokemon_edit_scene` with a
      return-scene hint of box. SELECT cycles boxes; D-pad up/down
      scrolls the slot list.
- [ ] Verify `pokemon_edit_scene` works identically from Party and Box
      entry points — one component, two callers, no per-caller logic.
- [ ] Update `tests/checklist.md` with box edit round-trips, including
      cross-box navigation and edit-then-back.

**Done when:** edit a box-resident Pokemon's species, save, reload in
original game → Pokemon matches; navigating boxes works in both
directions.

---

## Checkpoint #6 — Sprites (Option C)

- [ ] `assets/silhouette.png` — generic 64×64 silhouette, drawn by us.
- [ ] butano asset import for the silhouette; render it on the Pokemon
      editor and Party/Box list right panels.
- [ ] `tools/extract_sprites.py` — accepts `--rom /path/to/gen3.gba`,
      dumps 387 64×64 front sprites to `frontend-gba/assets/sprites/`.
      No-op if directory already populated.
- [ ] Makefile target: `make gba SPRITES_ROM=/path/to/rom.gba` runs the
      extractor before invoking the butano build.
- [ ] README section documenting the extractor's existence, legal
      caveat, and how to use it.
- [ ] Pokemon editor right panel: if extracted sprite exists for the
      current species, render it; else render the silhouette.

**Done when:** `make gba` with no args still ships a working ROM (with
silhouettes); `make gba SPRITES_ROM=...` produces a ROM where the
Pokemon editor right panel shows the real sprite for the current
species.

---

## mGBA Lua automation — the only real frontend verification layer

Originally deferred to post-GBA work.  **Promoted to load-bearing**
because manual emulator runs aren't happening per merge — without this
harness, every frontend change ships unverified at the emulator level.

### Why now

- libpksave Unity covers byte-level correctness (encoder, PID-derive,
  recalc, set/get round-trips).  It cannot catch scene-loop bugs,
  FLASH bank-switch errors, save-blocks-input regressions, or the
  trainer-name-round-trips-on-a-real-cart end-to-end check.
- The CP#2 manual checklist already enumerates the scenarios that need
  this coverage — those rows become Lua test cases one-to-one.
- Building the harness during CP#3 (the largest UI checkpoint) doubles
  as the moment the test surface is broadest and the regression risk
  is highest.

### Scope of the POC (timeboxed: 2 days)

- [ ] **`docker/test-gba` container** with headless mGBA built
      `-DUSE_LUA=ON` (or Debian's `mgba-sdl` if it ships Lua).
- [ ] **Python harness** in `frontend-gba/tests/automation/` that
      spawns mGBA with `--script <test>.lua`, captures the SRAM dump
      written by the Lua script, decodes via libpksave (`ctypes` over
      the same `.a` Unity uses), and asserts.
- [ ] **One end-to-end Lua test** proving the harness: boot ROM, drive
      cursor to Trainer, enter a known name via text_input, Save & Exit,
      dump SRAM, decode, assert trainer.name matches.  Per-variant
      fixtures (RS / Emerald / FRLG) — same fixture story as the
      libpksave Unity suite.
- [ ] **`make test-gba` target** wired into the root Makefile alongside
      `make test`.

### Scope of full coverage (incremental, post-POC)

- One Lua case per box ticked in `frontend-gba/tests/checklist.md`.
- SRAM-roundtrip assertions cover any test that touches the save buffer
  (trainer fields, party slot edits, bag changes, box edits — most of
  the checklist).
- A small number of tile-pattern / framebuffer-byte assertions cover
  the cases SRAM can't reach: "dirty indicator appears", "save scene
  blocks input", "cursor wraps at list ends".
- Per-test wall time budget: <30s.  Full suite target: <10 min.

### Constraints / known risks

- mGBA Lua API is small and moderately stable; button-timing across
  frame boundaries is the main source of script fragility.  Mitigate by
  centralising `wait_frames(n)` and `tap(button)` helpers — when butano
  debounce changes, you tune two helpers instead of N scripts.
- Headless mGBA in docker needs explicit `--frameskip` / SDL-null;
  verify in the POC before scaling out.
- The harness should fail loudly if mGBA crashes mid-test (no silent
  green-on-skipped).

### Relationship to the manual checklist

- `frontend-gba/tests/checklist.md` stays in the repo as the
  user-facing verification doc and as the wishlist of scenarios this
  harness should eventually cover.  Each Lua test in
  `frontend-gba/tests/automation/` should reference the checklist row
  it discharges so the two stay synchronised.

---

## Acceptance criteria (v1 complete)

The maintainer doesn't run manual mGBA checks per merge, so criteria
that rely on a human driving the emulator have been recast as
machine-checkable equivalents (Unity for byte-level correctness; the
mGBA Lua harness for scene-loop and save-flow).  The user-facing
manual checklist in `frontend-gba/tests/checklist.md` stays in the
repo but is not on this list.

- [ ] `make gba` (no args) produces `pksave-gba.gba` with silhouettes only.
- [ ] `make gba SPRITES_ROM=...` produces a ROM with real sprites.
- [ ] `make test` green (libpksave Unity tests including all D-* and L-*
      helpers added during this work).
- [ ] `make test-gba` green — mGBA Lua harness covers:
      - boot + variant detection on RS / Emerald / FRLG fixtures
      - UNKNOWN-content fixture lands on the boot error scene
      - SRAM-roundtrip after Save & Exit for at least one field per CP
        (trainer name, party-slot species, bag item quantity,
        box-slot species)
      - dirty indicator appears after an edit and clears after save
      - save scene blocks input while the FLASH write is in flight
- [ ] `docs/frontend-gba-libpksave-gaps.md` reflects which gaps were
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

## What this plan does not do

- Does not extend `libpksave` for Gen IV/V — those gaps are the Gen IV
  developer's discovery loop.
- Does not surface game flavor (types, item descriptions, ability
  descriptions, named badges, nature stat modifiers). See the scoping
  principle in the gaps matrix.
- Does not fill L8 / L9 (badge names, nature modifiers). Filed as known
  library gaps for future consumers.
