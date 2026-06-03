<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# Coverage matrix

What the library currently exposes, per generation. Anything marked
"absent" is a feature that does not exist in that generation of game.
For features the games have but the library does not yet read/write,
see [unsupported-features.md](unsupported-features.md).

Legend: ✅ supported · ⛔ absent in game · ❌ not implemented

## API coverage

| Feature                       | Gen I (RB/Y)        | Gen II (GS/C)       | Gen III (RS/E/FRLG) | Gen IV (DP/Pt/HGSS) | Gen V (BW/BW2)      |
|-------------------------------|---------------------|---------------------|---------------------|---------------------|---------------------|
| Save detection                | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Checksum recompute on flush   | ✅ (one's-comp byte)| ✅ (LE16 ×1 / ×2)   | ✅ (per-section)    | ✅ (CRC-16-CCITT)   | ✅ (CRC-16-CCITT)   |
| Trainer name / TID / SID      | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Trainer money                 | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Badges (bitfield)             | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Party read / slot write       | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Box count                     | 12                  | 14                  | 14                  | 18                  | 24                  |
| Box read / slot write         | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Pokémon nickname              | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Pokémon OT name               | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Species / level / moves+PP    | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| EVs / stat experience         | ✅ (stat exp 0-65535)| ✅ (stat exp 0-65535)| ✅ (EV 0-255)       | ✅ (EV 0-255)       | ✅ (EV 0-255)       |
| IVs / DVs                     | ✅ (DV→IV ×2)       | ✅ (DV→IV ×2)       | ✅                  | ✅                  | ✅                  |
| Held item                     | ⛔                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Nature                        | ⛔                  | ⛔                  | ✅                  | ✅                  | ✅                  |
| Ability slot                  | ⛔                  | ⛔                  | ✅                  | ✅                  | ✅                  |
| Shininess (`is_shiny`)        | ✅ (derived from DVs)| ✅ (derived from DVs)| ✅                  | ✅                  | ✅                  |
| Calculated stats (read-only)  | ✅                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Pokérus (party + box)         | ⛔                  | ✅                  | ✅                  | ✅                  | ✅                  |
| Name lookup: species / move / item | ✅              | ✅                  | ✅                  | ✅                  | ✅                  |
| Name lookup: ability          | ⛔                  | ⛔                  | ✅                  | ✅                  | ✅                  |
| Name lookup: nature           | ⛔                  | ⛔                  | ✅ (shared table)   | ✅                  | ✅                  |

## Bag pockets

`pksave_get_pocket()` will set `count = 0` and return `PKSAVE_OK` for any
pocket that does not exist in the detected game. `pksave_set_item()`
returns `PKSAVE_ERR_GAME` for an unsupported pocket.

| Pocket               | Gen I  | Gen II  | Gen III  | Gen IV  | Gen V  |
|----------------------|--------|---------|----------|---------|--------|
| `POCKET_ITEMS`       | ✅     | ✅      | ✅       | ✅      | ✅     |
| `POCKET_BALLS`       | ⛔ (mixed into Items) | ✅ | ✅ | ✅ | ✅ |
| `POCKET_KEY`         | ⛔ (mixed into Items) | ✅ | ✅ | ✅ | ✅ |
| `POCKET_TM_HM`       | ⛔ (mixed into Items) | ❌ not yet wired | ✅ | ✅ | ✅ |
| `POCKET_BERRIES`     | ⛔                    | ❌ not yet wired | ✅ | ✅ | ✅ |

Gen I genuinely has only one bag pocket; the in-game UI doesn't separate
Balls / Key / TM-HM. The Gen II ❌ entries are present in the games but
not yet wired through `gen2_pocket_layout()` — see
[unsupported-features.md](unsupported-features.md).

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
