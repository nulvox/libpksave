<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# frontend-gba manual test checklist

> **Status — read this first.** This checklist is **not** the project's
> verification gate.  The maintainer ships frontend changes without
> running it: the manual cost (boot ROM in mGBA, drive each scene by
> hand, verify on the original Pokémon ROM) is too high to repeat per
> merge.  Frontend behaviour is therefore **unverified at the emulator
> level** unless and until the mGBA-Lua automation work lands — see
> `docs/frontend-gba-plan.md` §"mGBA Lua automation" for the planned
> SRAM-roundtrip harness that will replace the load-bearing parts of
> this document.
>
> What this file *is* good for:
>   - **End users** verifying their own build of `pksave-gba.gba`
>     before trusting it with a real save.
>   - **Contributors** wanting a regression-list to spot-check a PR.
>   - **A wishlist for the mGBA automation harness** — every box here
>     is a scenario the Lua harness should eventually cover.

Each top-level section corresponds to one milestone in
`docs/frontend-gba-plan.md`.  Tick a box when the check passes on
your hardware/emulator.

Fixtures live in `tests/fixtures/` (see that directory's README) and
must be supplied by the user — no fixture is committed.

---

## Checkpoint #1 — boot, detect, error, main menu shell

Run `make gba` at the repo root.  Build artifact must land at
`bin/pksave-gba.gba`.

- [ ] `make gba` exits 0 and produces `bin/pksave-gba.gba`.
- [ ] mGBA loads the ROM with `ruby.sav` (128 KB) symlinked to the
      cartridge save slot — main menu appears, header reads
      `Ruby/Sapphire`.
- [ ] mGBA loads the ROM with `emerald.sav`     — header reads `Emerald`.
- [ ] mGBA loads the ROM with `firered.sav`     — header reads
      `FireRed/LeafGreen`.
- [ ] Loading the ROM with a 128 KB file full of random bytes (or any
      non-Gen-III save) lands on the boot error screen displaying
      `Unknown save format` or `Not a Gen III save`.
- [ ] Loading the ROM with a syntactically-Gen-III save whose checksums
      have been corrupted lands on the boot error screen displaying
      `Save corrupt`.
- [ ] D-pad up / down moves the menu cursor between five entries
      (Trainer / Party / Bag / Boxes / Save & Exit) and wraps at the
      ends.
- [ ] A on any menu entry is a no-op this checkpoint (target scenes
      land in Checkpoint #2+).
- [ ] L / R shoulders are no-ops this checkpoint (other top-level
      pages don't exist yet).
- [ ] The "*" dirty indicator does NOT appear in this checkpoint —
      nothing mutates `save_buf` yet.  (Activated in Checkpoint #2 once
      the trainer scene starts editing fields.)

---

## Checkpoint #2 — Trainer screen + Save & Exit + text input

Build artifact must land at `bin/pksave-gba.gba`.  The `*` dirty
indicator on the main-menu header must appear after the first edit and
clear after Save & Exit returns.  Round-trips are verified by loading
the original Pokemon ROM with the edited save and reading the value in
the in-game menu.

### Build & charset

- [ ] `make gba` exits 0 and produces `bin/pksave-gba.gba`.
- [ ] `make test` runs the Gen 3 charset round-trip test
      (`tests/test_gen3_charset`); all cases pass for every character
      the keyboard exposes (A-Z, a-z, 0-9, the eight punctuation marks
      `! ? . - , / ' "`, and space).  Includes
      `test_gen3_encoder_canonical_bytes` which proves the encoded
      bytes match the pret-canonical Gen 3 charmap (not just that the
      encoder is self-consistent with the decoder).

### Trainer screen — navigation

- [ ] From the main menu, A on `Trainer` opens the Trainer screen.
- [ ] B on the Trainer screen returns to the main menu (selection
      cursor preserved).
- [ ] D-pad up / down moves between rows: Name, TID, SID, Money,
      Badge 1..Badge 8.  Selection wraps at top and bottom; the list
      auto-scrolls so the focused row stays in the viewport.

### Trainer name round-trip

- [ ] On Trainer screen, focus `Name` and press A — the text-input
      keyboard appears, pre-populated with the current name.
- [ ] SELECT cycles tabs (UPPER / lower / sym); START commits;
      B deletes last char; B on empty buffer cancels.
- [ ] After committing a new name (e.g. `TESTER`), the Trainer screen
      shows the new name and the main-menu `*` dirty indicator appears.
- [ ] Choose `Save & Exit` from the main menu — the `SAVING... DON'T
      TURN OFF THE POWER!` screen appears and ignores all input until
      the FLASH write completes.
- [ ] After save returns to the main menu the `*` dirty indicator is
      gone.
- [ ] Power-cycle the emulator, reload the original Pokemon ROM with
      the same save — the trainer name in the in-game menu equals
      `TESTER`.

### TID round-trip

- [ ] Focus TID row; D-pad right increments by 10 (with hold-to-accel),
      left decrements by 10.  The value never exits `[0, 65535]`.
- [ ] Edit, save, reload in the original game — TID matches.

### Money round-trip

- [ ] Focus Money row; D-pad right increments by 10 (with hold-to-accel
      ramping noticeably faster after ~1s).
- [ ] Set Money to a non-trivial value (e.g. 12345), save, reload in
      the original game — money in the trainer card matches.
- [ ] Money saturates at 999,999 (Gen 3 cap) when right is held with
      max accel.

### Badges round-trip

- [ ] Focus a `Badge N` row; A toggles the row between `ON` and `off`.
- [ ] Save, reload original game — the corresponding badge bit in the
      trainer card matches.

### Save scene blocks input

- [ ] During the SAVING screen, holding any of SELECT / START / A / B /
      D-pad has no observable effect (no scene transition, no UI
      change beyond the success-then-return one).
- [ ] If `flush_to_flash` fails, the scene shows `SAVE FAILED. Power
      off and retry.` and stays there.  (No reliable way to force this
      without a hardware fault; record `n/a` if unable to test.)

### Dirty indicator

- [ ] Fresh boot: main menu has no `*` on the header.
- [ ] After any field edit (name, TID, SID, money, badge toggle):
      `*` appears on the Trainer header and remains visible on the
      main menu header after returning via B.
- [ ] After a successful Save & Exit: `*` is gone from the main menu.

---

## Checkpoint #3 — Party + Pokemon editor

To be filled in when Checkpoint #3 lands.

---

## Checkpoint #4 — Bag

To be filled in when Checkpoint #4 lands.

---

## Checkpoint #5 — Boxes

To be filled in when Checkpoint #5 lands.

---

## Checkpoint #6 — Sprites

To be filled in when Checkpoint #6 lands.
