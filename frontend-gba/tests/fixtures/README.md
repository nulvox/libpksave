<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# frontend-gba test fixtures

`frontend-gba` is tested manually against real Gen III `.sav` files
(strategy ii in `docs/frontend-gba-plan.md`).  Fixtures are **not**
committed — the user supplies their own.

## Expected filenames

Each fixture is exactly **131,072 bytes** (128 KB — full FLASH dump):

| File             | Game                          | Source                  |
|------------------|-------------------------------|--------------------------|
| `ruby.sav`       | Pokemon Ruby or Sapphire      | dump from real cartridge or in-game-saved emulator |
| `emerald.sav`    | Pokemon Emerald               | dump from real cartridge or in-game-saved emulator |
| `firered.sav`    | Pokemon FireRed or LeafGreen  | dump from real cartridge or in-game-saved emulator |

## How to use

Symlink one of these into the path mGBA expects next to your editor ROM
(usually `pksave-gba.sav` adjacent to `pksave-gba.gba`), then launch
mGBA.  The editor will read the FLASH at boot, run `pksave_detect`, and
land on the main menu with the game banner you'd expect.

## Why no committed fixtures

Real saves contain trainer names, IDs, and party data that belong to
their owners — even synthetic-looking ones are derived works of Game
Freak's save format.  Committing them risks both privacy and a derived-
work licensing question.  Generating synthetic fixtures that satisfy
all of pksave's checksum invariants (including the Gen III security
section) is roughly as much effort as a third frontend, so this repo
asks contributors to bring their own and gitignores the fixture
files.

## .gitignore policy

This directory's `README.md` is checked in.  Everything else in this
directory is gitignored (`*.sav`, `*.bin`).  See the repo-level
`.gitignore`.
