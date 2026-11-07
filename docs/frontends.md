<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# Frontends

`libpksave` is designed to be shared by small editor ROMs for each handheld
family. The Generation III GBA editor is implemented; the GBC and NDS
frontends remain placeholders.

## Status

| Frontend | Directory | Toolchain | State | Compile flags |
|---|---|---|---|---|
| GBC editor ROM | `frontend-gbc/` | gbdk-2020 4.3.0 | Planned placeholder | `-DPKSAVE_GEN1 -DPKSAVE_GEN2` |
| GBA editor ROM | `frontend-gba/` | Butano 18.5.0 / devkitARM | Implemented, with host and mGBA automation tests | `-DPKSAVE_GEN3` |
| NDS editor ROM | `frontend-nds/` | BlocksDS | Planned placeholder | `-DPKSAVE_GEN4 -DPKSAVE_GEN5` |

## GBA editor

The current editor supports Ruby, Sapphire, Emerald, FireRed, and LeafGreen
save data. Its scene-based UI covers boot-time detection, trainer fields,
party and box Pokémon, bag slots, Pokérus, save/flush handling, and optional
per-species sprites extracted from a user-supplied ROM.

Build and test it from the repository root:

```sh
make gba       # writes bin/pksave-gba.gba
make test-gba  # runs the headless mGBA/Lua automation suite
```

The test harness exercises ROM loading, save detection, navigation, field
round-trips, flash persistence, and editor scenes. See
[`frontend-gba/tests/checklist.md`](../frontend-gba/tests/checklist.md) for
additional manual verification scenarios and
[`frontend-gba-plan.md`](frontend-gba-plan.md) for implementation history.

## Shared frontend contract

Each frontend follows the same boundary:

1. Call `pksave_detect(sram, len)` before presenting editable data.
2. Read and write through `pksave_get_*` / `pksave_set_*`; frontend code does
   not own save offsets, binary layouts, or game character encodings.
3. Render numeric IDs through the library's species, move, item, ability, and
   nature lookup APIs where available.
4. Call `pksave_flush()` before persisting the caller-owned SRAM buffer so the
   format-specific checksums are rebuilt.
5. Treat Party and Box Pokémon as two entry points into the same editing model.

The original product requirements, technical specification, and agent build
brief are retained for project history under [`docs/internal/`](internal/).

## Planned platforms

### GBC — `frontend-gbc/`

The intended UI targets a 160×144 tile-based display and links only the Gen I
and II codecs. This directory currently contains no editor implementation, so
`make gbc` is reserved for future work.

### NDS — `frontend-nds/`

The intended UI uses a touch-oriented lower screen and a read-only summary on
the upper screen, linking only the Gen IV and V codecs. This directory
currently contains no editor implementation, so `make nds` is reserved for
future work.
