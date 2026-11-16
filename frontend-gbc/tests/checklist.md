<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 nulvox
-->

# frontend-gbc manual test checklist

> **Status — read this first.** Automated verification runs via
> `make test-gbc` (24 headless mGBA-Lua tests covering all 4 Gen I/II
> variants: Red/Blue, Yellow, Gold/Silver, Crystal). This checklist is
> provided for end users and contributors who want to manually verify
> `bin/pksave-gbc.gbc` in a desktop emulator against a real save file.

---

## Boot, detection & main menu

- [ ] `make gbc` exits 0 and produces `bin/pksave-gbc.gbc` (128 KiB CGB ROM).
- [ ] Loading `pksave-gbc.gbc` in mGBA with a Red/Blue, Yellow, Gold/Silver,
      or Crystal `.sav` (32 KiB) displays `POKEMON RED/BLUE`, `POKEMON YELLOW`,
      `POKEMON GOLD/SILV`, or `POKEMON CRYSTAL` on the header bar.
- [ ] Loading with an invalid or corrupted `.sav` lands on the `SAVE ERROR`
      halt screen without modifying SRAM.
- [ ] D-pad UP / DOWN moves the cursor across `Trainer`, `Party`, `Bag`,
      `Boxes`, and `Save & Exit`, wrapping at both ends.

## Trainer screen & text input

- [ ] Selecting `Trainer` displays `Name`, `TID`, `Money`, and `Badge 1..8`
      (Gen I) or `Badge 1..16` (Gen II Johto + Kanto).
- [ ] Pressing A on `Name` opens the 5×6 character-grid keyboard; `SELECT`
      cycles `UPPER` / `lower` / `sym` tabs, `A` appends, `B` deletes (or
      cancels when empty), and `START` commits.
- [ ] Pressing LEFT / RIGHT on `TID` or `Money` steps by ±10 with
      hold-to-accelerate.
- [ ] Pressing A on any Badge row toggles `ON` / `off`.
- [ ] Any edit adds `" *"` to the header bar; `Save & Exit` shows the
      `"SAVING... DON'T TURN OFF THE POWER!"` screen, recomputes checksums,
      and clears `" *"`.

## Party, Boxes & Pokémon editor

- [ ] `Party` lists the 6 party slots; `Boxes` lists the 20 slots in the
      current PC box (`SELECT` cycles boxes 1..12 in Gen I, 1..14 in Gen II).
- [ ] Pressing A on a slot in either `Party` or `Boxes` opens the shared
      3-tab Pokémon editor (`IDENTITY`, `STATS`, `MOVES`), cycled with
      LEFT / RIGHT in navigation mode.
- [ ] `IDENTITY` edits Species, Nickname, OT Name, Held Item (Gen II),
      Shiny DV toggle, and Pokérus modal (Gen II).
- [ ] `STATS` edits Level (1..100), Stat Exp (`EV HP..Spe`, 0..65535), and
      DVs (`DV Atk..Spe`, 0..15, with `DV HP` derived automatically) while
      recalculating the 6 stats live on rows 15–16.
- [ ] `MOVES` edits all 4 move slots and their PP (reseated to base PP on
      move change and capped at `base_pp * 8 / 5`).

## Bag & Slot editor

- [ ] `Bag` displays `ITEMS` (Gen I) or cycles `ITEMS`, `BALLS`, `KEY`,
      `TM/HM` with LEFT / RIGHT (Gen II).
- [ ] Pressing A on a bag slot opens `EDIT SLOT`, scoping the item cycler
      to the active pocket via `pksave_item_pocket` and clamping quantity
      via `pksave_item_max_stack`.
