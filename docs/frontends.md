<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# Frontends

> **TL;DR — all three frontends are TODO.**
>
> `frontend-gbc/`, `frontend-gba/`, and `frontend-nds/` currently contain
> only `.gitkeep`. No ROM has been built. The library is ready to be
> linked against; the UI shells around it have not been written.

## Status today

| Frontend       | Directory       | Toolchain       | State            | Compile flags                  |
|----------------|-----------------|-----------------|------------------|--------------------------------|
| GBC editor ROM | `frontend-gbc/` | gbdk-2020 4.3.0 | empty placeholder | `-DPKSAVE_GEN1 -DPKSAVE_GEN2`  |
| GBA editor ROM | `frontend-gba/` | butano 18.5.0   | empty placeholder | `-DPKSAVE_GEN3`                |
| NDS editor ROM | `frontend-nds/` | BlocksDS        | empty placeholder | `-DPKSAVE_GEN4 -DPKSAVE_GEN5`  |

`make gbc` / `make gba` / `make nds` build the Docker images successfully
but the containers run `make -C /src/frontend-*` against directories that
contain no Makefile, so the build is a no-op (or, depending on `make`'s
mood, an error). This is expected until each frontend is scaffolded.

The CI workflow does **not** invoke these targets for the same reason —
see [build.md](build.md).

## What the frontends are supposed to do

All three follow the same logical structure (from [SPEC.md](../SPEC.md)
and [AGENT_PROMPT.md](../AGENT_PROMPT.md)):

1. **Boot** — call `pksave_detect(sram, len)`. If `UNKNOWN`, show an
   error screen and halt.
2. **Main menu** — Trainer → Party → Bag → Boxes.
3. **Edit screens** — every value comes from a `pksave_get_*` call;
   every edit goes back through a `pksave_set_*` call. The frontend
   owns no save-format knowledge.
4. **Commit** — call `pksave_flush()` when the user exits an edit screen
   (not on every keystroke). Flush recomputes all checksums.
5. **Display** — render IDs through `pksave_species_name` /
   `pksave_move_name` / `pksave_item_name`. Never show raw numeric IDs.

The Party and Box editors must share the same Pokémon-editing component —
they are entry points into the same UI, not separate implementations.

## Per-platform UX (planned)

### GBC — `frontend-gbc/` (gbdk-2020, C)

- 160 × 144, GBC enhanced palettes (not DMG-monochrome).
- Tile-based list menus. D-pad scrolls, A confirms, B cancels.
- Numeric fields edited with left/right (hold for acceleration).
- Species / move / item fields cycle by name via lookup tables.
- No Pokémon sprites — Pokéball / type icon tiles at most.

### GBA — `frontend-gba/` (butano, C++)

- 240 × 160. Two-panel layout: scrollable list left (~120 px), detail
  panel right (~120 px).
- Party screen renders 64 × 64 front sprite on the detail side; sprite
  sheet embedded in the ROM.
- L / R shoulders cycle top-level sections; A confirms, B cancels.
- butano is C++; libpksave is linked as a static C library and called
  via `extern "C"`.

### NDS — `frontend-nds/` (BlocksDS, C)

- Bottom screen: interactive list, touch targets ≥ 24 px tall, both
  stylus and D-pad navigation.
- Top screen: read-only summary of the selected entity (Pokémon sprite +
  nickname + species + level + nature; or item name + quantity; or
  trainer name + badge count).
- Top and bottom update together on every selection change.

## How to start a frontend

Each frontend directory needs, at minimum:

- A `Makefile` that the matching `docker/<plat>/Dockerfile` `CMD` will
  invoke (`make -C /src/frontend-<plat> all`). It must produce
  `/src/bin/pksave-<plat>.<ext>`.
- Sources that `#include "pksave.h"` (path: `-I/src/libpksave/include`)
  and link the libpksave `.c` files into the ROM with the appropriate
  `-DPKSAVE_GENx` flags from [build.md](build.md).
- A boot sequence calling `pksave_detect` before anything else.

There is no compiled `libpksave.a` artifact today — each frontend
compiles `libpksave/src/*.c` directly. That keeps the per-platform build
simple and the per-platform `-DPKSAVE_GENx` selection consistent.
