<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# Architecture

## Repository layout

```
libpksave/
├── Makefile                            — top-level; delegates everything to Docker
├── docs/
│   └── internal/                       — archived product/spec/agent planning briefs
├── libpksave/                          — the library
│   ├── include/pksave.h                — public API (the only header callers include)
│   ├── src/
│   │   ├── detect.c                    — pksave_detect() — variant identification
│   │   ├── session.c                   — pksave_open/close/flush + public dispatch
│   │   ├── checksum.c                  — shared checksum entry points
│   │   ├── gen1.c … gen5.c             — per-generation parse / serialize / checksum
│   │   ├── pksave_internal.h           — gen-private declarations + shared helpers
│   │   ├── names.c                     — public name-lookup functions
│   │   └── names/genN_*.h              — species/move/item/ability tables
│   ├── Makefile                        — invoked inside docker/test
│   └── tests/                          — Unity tests + fixture .sav files
├── frontend-gbc/                       — placeholder (gbdk-2020, TODO)
├── frontend-gba/                       — implemented Gen III editor (butano)
├── frontend-nds/                       — placeholder (BlocksDS, TODO)
├── docker/{test,gbc,gba,nds}/          — one Dockerfile per toolchain
└── bin/                                — Docker build artifacts land here
```

## Public API surface

Callers include exactly one header: `pksave.h`. The header exposes:

- `pksave_detect(sram, len) → pksave_game_t`
- `pksave_open(sram, len, game) → pksave_save_t *`
- `pksave_close(save)` / `pksave_flush(save)`
- Trainer: `pksave_get_trainer` / `pksave_set_trainer`
- Party:   `pksave_get_party`   / `pksave_set_party_slot`
- Box:     `pksave_get_box_count` / `pksave_get_box` / `pksave_set_box_slot`
- Bag:     `pksave_get_pocket`   / `pksave_set_item`
- Pokérus: `pksave_get_party_pokerus` / `pksave_set_party_pokerus`
            `pksave_get_box_pokerus`   / `pksave_set_box_pokerus`
- Names:   `pksave_species_name`, `pksave_move_name`, `pksave_item_name`,
            `pksave_ability_name`, `pksave_nature_name`
- Test helper: `pksave_validate_checksum`

There is **no** dynamic allocation. `pksave_open` returns a pointer to one
static `struct pksave_save` slot owned by the library; the caller owns the
SRAM buffer it points into.

## Dispatch model

`session.c` is the entire public dispatch layer. Every public function is
a `switch (save->game)` that routes to a `genN_*` implementation in
`gen1.c` … `gen5.c`, with each arm guarded by `#ifdef PKSAVE_GENx` so that
generations the frontend doesn't need are dropped at link time.

The compile-time generation flags are set per-container so each ROM only
ships the code it can use:

| Container     | Compile flags                                             |
|---------------|-----------------------------------------------------------|
| `docker/test` | all five: `-DPKSAVE_GEN1 … -DPKSAVE_GEN5`                 |
| `docker/gbc`  | `-DPKSAVE_GEN1 -DPKSAVE_GEN2`                             |
| `docker/gba`  | `-DPKSAVE_GEN3`                                           |
| `docker/nds`  | `-DPKSAVE_GEN4 -DPKSAVE_GEN5`                             |

The per-generation `_get_*` / `_set_*` / `_flush` / `_validate_checksum`
prototypes are declared inside the matching `#ifdef PKSAVE_GENx` block in
`pksave_internal.h`, so calling a function for a generation that wasn't
compiled in is a link error rather than a runtime crash.

## Name tables

Name tables in `src/names/genN_*.h` are `const char * const[]` arrays,
each wrapped in `#ifdef PKSAVE_GENx`. They are not separately compiled —
`names.c` `#include`s the headers, so unused generations are stripped at
preprocess time. Lookup returns `"???"` for unknown / out-of-range IDs;
the returned pointer is into static read-only storage and must not be
freed.

## Checksum strategy

- **Gen I**: single byte at `0x3523`, one's-complement sum of `0x2598..0x3522`.
- **Gen II**: two LE16 sums (GS uses a different region than Crystal);
  Crystal also stores a secondary copy.
- **Gen III**: per-section 16-bit sum; section signature `0x08012025`
  identifies live sections inside the two ring-buffered save slots.
- **Gen IV / V**: CRC-16-CCITT over each block; helper `pksave_crc16_ccitt`
  in `pksave_internal.h` is shared by both.

All checksums are recomputed unconditionally by `pksave_flush()`.

## Why this shape

- **Single static save slot**: lets the library run on platforms with no
  heap (gbdk-2020 in particular).
- **Buffer aliasing the caller's SRAM**: the frontend hands us a pointer
  to SRAM and we mutate it in place — the emulator's battery-backup write
  is what persists the change. No staging buffer.
- **Per-gen `#ifdef` everywhere**: each frontend's ROM only ships the
  bytes it can possibly need. GBC ROMs do not link Gen V item tables.
- **No allocation, no I/O, no SDK headers**: the library is portable C99
  and links into any of the three toolchains without adapters.
