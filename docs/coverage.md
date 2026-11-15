<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 nulvox
-->

# Coverage matrix

What the library currently exposes, per generation. Anything marked
"absent" is a feature that does not exist in that generation of game.
For features the games have but the library does not yet read/write,
see [unsupported-features.md](unsupported-features.md).

Legend: ✅ supported · ⛔ absent in game · ❌ not implemented

## API coverage

| Feature                                  | Gen I (RB/Y)         | Gen II (GS/C)        | Gen III (RS/E/FRLG)  | Gen IV (DP/Pt/HGSS) | Gen V (BW/BW2)      |
|------------------------------------------|----------------------|----------------------|----------------------|---------------------|---------------------|
| Save detection                           | ✅                   | ✅                   | ✅                   | ✅                  | ✅                  |
| Checksum recompute on flush              | ✅ (Bank 1 byte)     | ✅ (LE16 ×1 / ×2)    | ✅ (per-section)     | ✅ (general block)  | ✅ (single block)   |
| Trainer name / TID / SID                 | ✅ (SID ⛔)          | ✅ (SID ⛔)          | ✅                   | ✅ (ASCII-in-u16)   | ✅                  |
| Trainer money                            | ✅                   | ✅                   | ✅                   | ✅                  | ✅                  |
| Badges (`uint8_t` bitfield)              | ✅ (8 Kanto)         | ✅ (8 Johto only)    | ✅ (8 Hoenn/Kanto)   | ✅ (8 Sinnoh/Johto) | ✅ (8 Unova)        |
| Party read / slot write                  | ✅                   | ✅                   | ✅                   | ✅                  | ✅                  |
| Box count                                | 12 (20 slots/box)    | 14 (20 slots/box)    | 14 (30 slots/box)    | 18 (30 slots/box)   | 24 (30 slots/box)   |
| Box read / slot write                    | ✅                   | ✅                   | ✅                   | ✅                  | ✅                  |
| Box Pokémon level & XP sync              | ✅ (`box_level` + XP)| ✅ (`0x1F` + XP)     | ✅ (XP↔level sync)   | ❌ (left 0)         | ❌ (left 0)         |
| Pokémon nickname                         | ✅                   | ✅                   | ✅                   | ❌                  | ❌                  |
| Pokémon OT name                          | ✅                   | ✅                   | ✅                   | ❌                  | ❌                  |
| Species / party level / moves+PP         | ✅                   | ✅                   | ✅ (moves ≤ 255)     | ✅ (moves ≤ 255)    | ✅ (moves ≤ 255)    |
| EVs / stat experience                    | ✅ (stat exp 0-65535)| ✅ (stat exp 0-65535)| ✅ (EV 0-255)        | ✅ (EV 0-255)       | ✅ (EV 0-255)       |
| IVs / DVs                                | ✅ (DV→IV ×2)        | ✅ (DV→IV ×2)        | ✅                   | ✅                  | ✅                  |
| Held item                                | ⛔                   | ✅                   | ✅                   | ✅                  | ✅                  |
| Nature                                   | ⛔                   | ⛔                   | ✅ (PID-synced)      | ❌                  | ❌                  |
| Ability slot                             | ⛔                   | ⛔                   | ✅                   | ❌                  | ❌                  |
| Shininess (`is_shiny`)                   | ✅ (read-only DV)    | ✅ (read-only DV)    | ✅ (PID-synced R/W)  | ❌                  | ❌                  |
| Party stats (`stat[6]` read/write)       | ✅                   | ✅                   | ✅                   | ✅                  | ✅                  |
| Stat recalculation (`pksave_recalc_stats`)| ✅ (DV + √StatExp)  | ✅ (DV + √StatExp)   | ✅ (IV + EV/4 ±10%)  | ❌                  | ❌                  |
| Pokérus (party + box)                    | ⛔                   | ✅                   | ✅                   | ✅                  | ✅                  |
| Name lookup: species / move / item       | ✅ (incl. HM/TM)     | ✅                   | ✅                   | ✅                  | ✅                  |
| Name lookup: ability                     | ⛔                   | ⛔                   | ✅                   | ❌ stub             | ❌ stub             |
| Name lookup: nature                      | ⛔                   | ⛔                   | ✅ (shared table)    | ✅ (shared table)   | ✅ (shared table)   |
| Species abilities (`pksave_species_abilities`)| ⛔              | ⛔                   | ✅                   | ❌                  | ❌                  |
| Move base PP (`pksave_move_base_pp`)     | ✅                   | ✅                   | ✅                   | ❌                  | ❌                  |
| Item max stack (`pksave_item_max_stack`) | ✅                   | ✅                   | ✅                   | ❌                  | ❌                  |
| Item pocket (`pksave_item_pocket`)       | ✅                   | ✅                   | ✅                   | ❌                  | ❌                  |
| Numeric limits (`pksave_iv_max`, `pksave_ev_*`)| ✅             | ✅                   | ✅                   | ✅                  | ✅                  |
| Name-length macros (`PKSAVE_GENx_*_MAX`) | ✅                   | ✅                   | ✅                   | ❌                  | ❌                  |

## Bag pockets

`pksave_get_pocket()` will set `count = 0` and return `PKSAVE_OK` for any
pocket that does not exist in the detected game. `pksave_set_item()`
returns `PKSAVE_ERR_GAME` for an unsupported pocket.

| Pocket               | Gen I  | Gen II  | Gen III  | Gen IV  | Gen V  |
|----------------------|--------|---------|----------|---------|--------|
| `POCKET_ITEMS`       | ✅     | ✅      | ✅       | ✅      | ✅     |
| `POCKET_BALLS`       | ⛔ (mixed into Items) | ✅ | ✅ | ✅ | ✅ |
| `POCKET_KEY`         | ⛔ (mixed into Items) | ✅ | ✅ | ✅ | ✅ |
| `POCKET_TM_HM`       | ⛔ (mixed into Items) | ✅ | ✅ | ✅ | ✅ |
| `POCKET_BERRIES`     | ⛔                    | ⛔ (mixed into Items) | ✅ | ✅ | ✅ |

Gen I genuinely has only one bag pocket; the in-game UI doesn't separate
Balls / Key / TM-HM. Gen II separates Items, Balls, Key Items, and TM/HM,
while Berries are regular items stored inside the Items pocket.

## Detection signals (for reference)

| Variant     | How it's identified                                        |
|-------------|------------------------------------------------------------|
| `RB`        | 32 KiB, Gen I checksum valid, Pikachu friendship byte = 0  |
| `YELLOW`    | 32 KiB, Gen I checksum valid, Pikachu friendship byte ≠ 0  |
| `GS`        | 32 KiB, GS checksum at `0x2D69` valid                      |
| `CRYSTAL`   | 32 KiB, Crystal checksum at `0x2D0D` valid                 |
| `RS`        | 128 KiB, section signature + game-code in save block       |
| `EMERALD`   | 128 KiB, section signature + game-code in save block       |
| `FRLG`      | 128 KiB, section signature + game-code in save block       |
| `DP`        | 512 KiB, footer block size = DP value                      |
| `PLATINUM`  | 512 KiB, footer block size = Platinum value                |
| `HGSS`      | 512 KiB, footer block size = HGSS value                    |
| `BW`        | 512 KiB, footer block size = BW value                      |
| `BW2`       | 512 KiB, footer block size = BW2 value                     |

If none of the above match, `pksave_detect()` returns
`PKSAVE_GAME_UNKNOWN` and frontends should display an error screen.
