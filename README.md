<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# libpksave

A portable C99 library for reading and writing Pokémon save files from
Generations I–V, intended to be linked into homebrew save-editor ROMs that
run inside an emulator. The user symlinks their game's `.sav` onto the
editor ROM's battery-backed SRAM slot; libpksave parses, mutates, and
re-checksums the buffer in place.

> **Status:** the library itself is implemented and tested for all twelve
> game variants. **None of the three frontends are written yet** — the
> `frontend-gbc/`, `frontend-gba/`, and `frontend-nds/` directories are
> empty placeholders. See [docs/frontends.md](docs/frontends.md).

---

## Table of contents

### Specifications

- [PRD.md](PRD.md) — product requirements: what the editor should do
- [SPEC.md](SPEC.md) — technical specification: repo layout, API,
  toolchains, format research notes
- [AGENT_PROMPT.md](AGENT_PROMPT.md) — build-order brief for the
  implementing agent

### Documentation

- [docs/architecture.md](docs/architecture.md) — repo layout, build flow,
  per-generation `#ifdef PKSAVE_GENx` partitioning
- [docs/build.md](docs/build.md) — building and running the test suite,
  Docker container conventions, CI overview
- [docs/coverage.md](docs/coverage.md) — per-generation feature coverage
  matrix (what works today)
- [docs/unsupported-features.md](docs/unsupported-features.md) — game
  features not covered by the library (deliberately out of scope or
  not-yet-implemented), with reasoning
- [docs/frontends.md](docs/frontends.md) — frontend implementation status
  (currently: all three are TODO)

### Source map

- [`libpksave/include/pksave.h`](libpksave/include/pksave.h) — public API
- [`libpksave/src/`](libpksave/src/) — implementation
  (`detect.c`, `session.c`, `checksum.c`, `names.c`, `gen1.c` … `gen5.c`)
- [`libpksave/src/names/`](libpksave/src/names/) — per-generation
  species / move / item / ability name tables
- [`libpksave/tests/`](libpksave/tests/) — Unity tests, one per generation,
  plus `tests/fixtures/` with one synthetic `.sav` per variant

---

## Supported game variants

| Generation | Variant enum     | Games                                  | Save size |
|------------|------------------|----------------------------------------|-----------|
| I          | `RB`, `YELLOW`   | Red, Blue, Yellow                      | 32 KiB    |
| II         | `GS`, `CRYSTAL`  | Gold, Silver, Crystal                  | 32 KiB    |
| III        | `RS`, `EMERALD`, `FRLG` | Ruby, Sapphire, Emerald, FireRed, LeafGreen | 128 KiB   |
| IV         | `DP`, `PLATINUM`, `HGSS` | Diamond, Pearl, Platinum, HeartGold, SoulSilver | 512 KiB   |
| V          | `BW`, `BW2`      | Black, White, Black 2, White 2         | 512 KiB   |

All twelve variants are auto-detected by `pksave_detect()` from raw SRAM
bytes; no user prompt or filename hint is required.

---

## Quick start

The host only needs Docker — no cross-compilers are installed locally.

```sh
make test     # run libpksave Unity tests for all five generations
make gbc      # build pksave-gbc.gbc  (placeholder — frontend not yet implemented)
make gba      # build pksave-gba.gba  (placeholder — frontend not yet implemented)
make nds      # build pksave-nds.nds  (placeholder — frontend not yet implemented)
make all      # test + gbc + gba + nds
make clean    # remove test binaries and bin/ artifacts
```

See [docs/build.md](docs/build.md) for details on the Docker containers,
the `-DPKSAVE_GENx` compile flags, and CI.

---

## What this repo is, in one paragraph

`libpksave` is the shared brain. The three frontends — when written — are
just UI shells that call `pksave_detect()` on boot, then route user input
through `pksave_get_*` / `pksave_set_*` accessors and call
`pksave_flush()` to recompute checksums before returning to the main menu.
No frontend ever touches save offsets, byte layouts, or character
encodings directly; the library owns all save-format knowledge.

## License

MIT. See SPDX headers at the top of every source file. Struct layouts
and offsets are derived from the [pret](https://github.com/pret) decomp
projects (factual data, no code copied).
