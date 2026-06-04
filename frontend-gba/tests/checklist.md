<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# frontend-gba manual test checklist

Manual checks per checkpoint.  Each top-level section corresponds to
one milestone in `docs/frontend-gba-plan.md`.  Tick a box when the
check passes on your hardware/emulator.

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

To be filled in when Checkpoint #2 lands.

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
