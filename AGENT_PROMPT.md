# Agent Prompt: Build gbsdk Pokemon Save Editor

## What You Are Building

A multi-platform Pokemon save editor that runs as a homebrew ROM inside emulators. Three ROMs target three platforms (GBC, GBA, NDS) and share a single portable C library (`libpksave`) for all save format logic. All architecture decisions have been made — your job is to implement, not to design.

Read `PRD.md` for feature requirements and `SPEC.md` for the full technical specification before writing any code.

## The Core Constraint

The editor accesses save data through its own battery-backed SRAM. The user symlinks their Pokemon game's `.sav` file to the path the emulator uses for the editor ROM's SRAM slot. The editor reads from and writes to that SRAM buffer in-place. There is no filesystem access, no file picker, and no SD card. `libpksave` must recalculate all checksums on every flush so the save remains valid when loaded back into the original game.

## Build Order

Work in this order. Do not skip ahead.

1. **Scaffold the monorepo** — create the directory structure from `SPEC.md`. Add a root `Makefile` with `test`, `gbc`, `gba`, `nds`, `all`, and `clean` targets. Each target delegates to Docker.

2. **Write the Docker containers** — one per toolchain (`docker/test`, `docker/gbc`, `docker/gba`, `docker/nds`). Volume-mount `/src` (repo root) and write output to `/src/bin/`. The `docker/test` container uses standard gcc + Unity. Do not install toolchain dependencies on the host.

3. **Implement `libpksave`** — work generation by generation in this order: Gen I → Gen II → Gen III → Gen IV → Gen V. For each generation:
   - Implement detection in `detect.c`
   - Implement parse/serialize in `genN.c`
   - Implement checksum in `checksum.c`
   - Add name lookup tables in `names/genN_*.h` guarded by `#ifdef PKSAVE_GENx`
   - Write Unity tests in `tests/test_genN.c` using fixture saves

4. **Implement `frontend-gbc`** — GBC ROM using gbdk-2020. Tile-based list UI. Links `libpksave` built with `-DPKSAVE_GEN1 -DPKSAVE_GEN2`.

5. **Implement `frontend-gba`** — GBA ROM using butano. Two-panel UI with sprite previews. `libpksave` called via `extern "C"`. Built with `-DPKSAVE_GEN3`.

6. **Implement `frontend-nds`** — NDS ROM using BlocksDS. Top/bottom screen split, touch UI. Built with `-DPKSAVE_GEN4 -DPKSAVE_GEN5`.

## Format Research Sources

Use these sources to derive every byte offset, struct layout, and checksum algorithm. Do NOT copy code — read them as documentation and implement from scratch in C.

**Primary (authoritative):**
- Gen I: https://github.com/pret/pokered
- Gen II: https://github.com/pret/pokecrystal
- Gen III: https://github.com/pret/pokeemerald, https://github.com/pret/pokeruby, https://github.com/pret/pokefirered
- Gen IV: https://github.com/pret/pokeplatinum, https://github.com/pret/pokegold

The pret repos contain the actual struct definitions and save block layouts in their C headers. Look in `include/` and `src/` directories for save-related files (`save.h`, `save.c`, `pokemon.h`, etc.).

## libpksave Implementation Rules

- C99 only. No dynamic allocation (`malloc`/`free`). No platform SDK headers.
- `pksave_open` takes a pointer directly into the caller's SRAM buffer. Edits mutate that buffer. `pksave_flush` finalizes checksums in-place.
- All name lookup tables are `const char * const[]` in `names/genN_*.h`, guarded by `#ifdef PKSAVE_GENx`.
- Return `PKSAVE_ERR_*` codes on failure (defined in `pksave.h`). Never silently ignore errors.
- Gen I/II use different character encodings than Gen III+. Decode to ASCII in `pksave_get_*` and encode back in `pksave_set_*`. The pret repos have the character tables.
- Gen I/II EVs are "stat experience" (0-65535 per stat). Map them to the `evs[6]` field as-is — do not normalize to 0-255. The frontend must handle the display difference.
- Gen I/II have no natures or abilities. Set `nature = 0`, `ability = 0` in the output struct. `pksave_set_party_slot` should ignore nature/ability for Gen I/II saves.
- Shininess in Gen I/II is derived from DVs (IVs), not a flag. Compute it in `pksave_get_*` and expose as `is_shiny`. The frontend should not recompute it.

## Frontend Rules

- Frontends call `pksave_detect` on boot. If `PKSAVE_GAME_UNKNOWN`, display an error screen and halt.
- Call `pksave_flush` when the user exits an edit screen (not on every keypress).
- Do not reimplement any save logic. Every field value comes from a `pksave_get_*` call. Every edit goes through a `pksave_set_*` call.
- Use `pksave_species_name`, `pksave_move_name`, `pksave_item_name` for all display strings — never show raw numeric IDs to the user.
- The Party and Box editors must share the same Pokemon editing component/screen. Party editing and Box editing are entry points into the same UI, not separate implementations.

## GBC Frontend (gbdk-2020)

- Target: Game Boy Color. Use GBC-enhanced palettes; do not target DMG.
- Tile-based list menus. Top-level: Trainer → Party → Bag → Boxes.
- D-pad scrolls lists. A confirms. B cancels / returns to previous screen.
- Numeric fields (money, quantity, EVs, IVs): left/right to increment/decrement. Hold for acceleration.
- Species, move, item fields: left/right cycles through valid IDs by name.
- No Pokemon sprites. Use a small Pokeball tile or type icon as a list prefix at most.

## GBA Frontend (butano)

- butano is a C++ framework. Link `libpksave` as a static C library with `extern "C"` linkage.
- Two-panel layout: left panel ~120px wide (scrollable list), right panel ~120px wide (detail view).
- On the Party screen, render the selected Pokemon's 64×64 front sprite in the detail panel. Sprite assets must be embedded in the ROM — do not load from filesystem.
- L shoulder: cycle to previous top-level section. R shoulder: cycle to next.
- A confirms, B cancels, D-pad navigates.
- Source sprites from the pret repos or equivalent open-licensed Pokemon sprite sheets at the correct resolution for each generation.

## NDS Frontend (BlocksDS)

- Bottom screen is the interactive surface. Top screen is read-only.
- Bottom screen: scrollable list with touch targets at minimum 24px tall. All navigation available by both stylus tap and D-pad.
- Top screen: When a Pokemon is selected, show its front sprite, nickname, species name, level, and nature. When a bag item is selected, show item name and quantity. When trainer profile is active, show trainer name and badge count.
- Top and bottom screens update together on every selection change.
- Touch input: tap to select, drag to scroll, tap confirm/cancel buttons.

## Testing Requirements

- Every `libpksave` function must have at least one Unity test.
- Each format variant must have a fixture `.sav` in `tests/fixtures/`. Generate synthetic fixtures programmatically if real saves cannot be distributed — the generation script should be committed alongside the fixtures.
- The canonical test pattern for each variant:
  1. Open fixture → assert known trainer name, money, badge count
  2. Read party slot 0 → assert species ID, level, first move
  3. Mutate trainer money → flush → re-open same buffer → assert new money
  4. Assert checksum validates after flush (implement a `pksave_validate_checksum` helper for tests)
- `make test` must exit non-zero if any Unity test fails.

## License

MIT. Every source file must have the MIT license header. Do not copy code from any GPL-licensed source. Struct layouts and byte offsets derived from pret repos are facts, not copyrightable expression — they are safe to use.

## What Done Looks Like

- `make all` completes without errors
- `make test` passes all Unity tests for all five generations
- `pksave-gbc.gbc` loads in mGBA with a symlinked Gen I or Gen II `.sav`, detects the game, and allows editing trainer profile, party, bag, and boxes with correct round-trip behavior
- `pksave-gba.gba` loads in mGBA with a symlinked Gen III `.sav` and does the same
- `pksave-nds.nds` loads in melonDS with a symlinked Gen IV or Gen V `.sav` and does the same
- After editing and saving, loading the original Pokemon ROM with the same `.sav` shows the edited values and reports no save corruption
