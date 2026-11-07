<!--
SPDX-License-Identifier: MIT
Copyright (c) 2026 gbsdk contributors
-->

# libpksave

`libpksave` is a portable C99 library for detecting, reading, editing, and
checksum-validating binary Pokémon save files from Generations I–V. It keeps
save-format details behind one public header so host tools and emulator-ready
editors can work with trainer, party, box, bag, and Pokérus data without
hard-coding offsets or checksum rules.

The library is implemented and fixture-tested for twelve game variants. A GBA
editor demonstrates the API against Generation III saves; GBC and NDS
frontends remain planned.

## Overview and design goals

- **Portable C99:** no SDK headers, heap allocation, or I/O in the library.
  Callers provide a mutable save buffer and remain responsible for persistence.
- **One public API:** consumers include only
  [`libpksave/include/pksave.h`](libpksave/include/pksave.h).
- **Format isolation:** generation-specific parsing, character encoding, data
  layout, and checksum logic stay inside `libpksave/src/`.
- **Small target builds:** `PKSAVE_GEN1` through `PKSAVE_GEN5` compile-time
  flags allow embedded frontends to include only the formats they need.
- **In-place editing:** sessions alias the caller-owned save buffer; a
  successful `pksave_flush()` serializes pending data and recalculates the
  format's checksums.
- **Testable behavior:** synthetic fixtures cover detection, read/write
  round-trips, and post-flush checksum validation for every supported variant.

## Minimal C API example

This host-side example loads a save, detects its format, changes the trainer's
money, recalculates checksums, and writes the same buffer back to disk:

```c
#include <stdint.h>
#include <stdio.h>
#include "pksave.h"
int main(void) {
    uint8_t bytes[512 * 1024];
    FILE *fp = fopen("game.sav", "rb");
    if (!fp) return 1;
    size_t len = fread(bytes, 1, sizeof bytes, fp);
    fclose(fp);

    pksave_game_t game = pksave_detect(bytes, len);
    if (game == PKSAVE_GAME_UNKNOWN) return 1;
    pksave_save_t *save = pksave_open(bytes, len, game);
    pksave_trainer_t trainer;
    if (!save || pksave_get_trainer(save, &trainer) != PKSAVE_OK) return 1;
    trainer.money = 999999;
    if (pksave_set_trainer(save, &trainer) != PKSAVE_OK ||
        pksave_flush(save) != PKSAVE_OK) return 1;

    fp = fopen("game.sav", "wb");
    if (!fp || fwrite(bytes, 1, len, fp) != len) return 1;
    fclose(fp);
    pksave_close(save);
    return 0;
}
```

Compile the library with the generation flags required by your application;
the repository's Makefiles demonstrate complete host and embedded builds.
Back up real save files before editing them.

## Supported formats and status

| Generation | Games / variants | Save size | Core data support | Checksum support | Current limitations |
|---|---|---:|---|---|---|
| I | Red/Blue (`RB`), Yellow (`YELLOW`) | 32 KiB | Trainer, party, boxes, single bag pocket | One's-complement byte | Held items, nature, abilities, and separate pockets do not exist in-game |
| II | Gold/Silver (`GS`), Crystal (`CRYSTAL`) | 32 KiB | Trainer, party, boxes, Items/Balls/Key pockets, Pokérus | LE16 sums, including Crystal's secondary copy | TM/HM and Berry pockets are not yet wired |
| III | Ruby/Sapphire (`RS`), Emerald, FireRed/LeafGreen (`FRLG`) | 128 KiB | Full public read/write API and lookup tables | Per-section checksums across rotating save slots | GBA editor is implemented; emulator-level coverage continues to expand |
| IV | Diamond/Pearl (`DP`), Platinum, HeartGold/SoulSilver (`HGSS`) | 512 KiB | Trainer, party, boxes, bag, Pokérus | CRC-16-CCITT block checksums | Ability-name lookup is currently a stub |
| V | Black/White (`BW`), Black 2/White 2 (`BW2`) | 512 KiB | Trainer, party, boxes, bag, Pokérus | CRC-16-CCITT block checksums | Ability-name lookup is currently a stub |

All variants are detected from raw save bytes by `pksave_detect()`; callers do
not need to supply a filename hint. See
[`docs/coverage.md`](docs/coverage.md) for the per-feature matrix and
[`docs/unsupported-features.md`](docs/unsupported-features.md) for explicit
non-goals and remaining gaps.

## Build and test

The reproducible top-level workflow uses Docker for C and cross-compilation
toolchains. Python 3 is also used by asset and fixture tooling.

```sh
# Build the test image and run all Unity tests for Generations I-V
make test

# Run pure-Python tests for sprite tooling
make test-tools

# Build the implemented Generation III editor ROM
make gba
# output: bin/pksave-gba.gba

# Run the headless mGBA/Lua frontend automation suite
make test-gba
```

The GBA build can optionally extract sprites from a legally obtained
Generation III ROM without committing game assets:

```sh
make gba SPRITES_ROM=/path/to/your/game.gba
```

`make gbc` and `make nds` reserve the builds for the planned GBC and NDS
editors; those frontend directories are currently placeholders. `make all`
invokes every platform target, so use the individual commands above for the
currently implemented and tested workflow.

For a direct host build of the shared library (GCC required):

```sh
make -C libpksave lib
# output: libpksave/libpksave.so
```

See [`docs/build.md`](docs/build.md) for container/toolchain details and
[`docs/frontends.md`](docs/frontends.md) for frontend status.

## Repository map

- [`libpksave/include/pksave.h`](libpksave/include/pksave.h) — public API
- [`libpksave/src/`](libpksave/src/) — detection, dispatch, per-generation
  codecs, checksums, limits, and lookup tables
- [`libpksave/tests/`](libpksave/tests/) — Unity tests and generated synthetic
  save fixtures
- [`frontend-gba/`](frontend-gba/) — Butano-based Generation III editor ROM
- [`docker/`](docker/) — reproducible host-test and cross-compilation images
- [`docs/`](docs/) — architecture, coverage, build, and frontend documentation

## License

Source files carry MIT SPDX identifiers. No standalone `LICENSE` file is
currently included.
