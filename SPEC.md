# Technical Specification: libpksave Pokemon Save Editor

## Repository Layout

```
libpksave/
├── libpksave/
│   ├── include/
│   │   └── pksave.h
│   ├── src/
│   │   ├── detect.c
│   │   ├── checksum.c
│   │   ├── gen1.c
│   │   ├── gen2.c
│   │   ├── gen3.c
│   │   ├── gen4.c
│   │   ├── gen5.c
│   │   └── names/
│   │       ├── gen1_species.h   ← #ifdef PKSAVE_GEN1
│   │       ├── gen1_moves.h
│   │       ├── gen1_items.h
│   │       └── ... (one triplet per generation)
│   └── tests/
│       ├── fixtures/            ← one .sav per format variant
│       ├── test_detect.c
│       ├── test_gen1.c
│       ├── test_gen2.c
│       ├── test_gen3.c
│       ├── test_gen4.c
│       └── test_gen5.c
├── frontend-gbc/                ← gbdk-2020 project
├── frontend-gba/                ← butano project (C++)
├── frontend-nds/                ← BlocksDS project
├── docker/
│   ├── test/Dockerfile          ← gcc + Unity test framework
│   ├── gbc/Dockerfile           ← gbdk-2020 toolchain
│   ├── gba/Dockerfile           ← devkitPro + butano
│   └── nds/Dockerfile           ← BlocksDS toolchain
└── Makefile                     ← top-level: test, gbc, gba, nds, all, clean
```

## Build System

The root `Makefile` delegates all compilation to Docker. No cross-compiler is required on the host — only Docker.

```
make test     # runs libpksave Unity tests in docker/test container
make gbc      # builds pksave-gbc.gbc in docker/gbc container
make gba      # builds pksave-gba.gba in docker/gba container
make nds      # builds pksave-nds.nds in docker/nds container
make all      # test + gbc + gba + nds
```

Each Docker container volume-mounts the repo root at `/src` and writes build output to `/src/bin/`. Source is never copied into the image.

### Compile-time Generation Flags

Each platform container passes only the flags it needs:

| Container | Flags |
|---|---|
| `docker/test` | all five `-DPKSAVE_GENx` flags |
| `docker/gbc` | `-DPKSAVE_GEN1 -DPKSAVE_GEN2` |
| `docker/gba` | `-DPKSAVE_GEN3` |
| `docker/nds` | `-DPKSAVE_GEN4 -DPKSAVE_GEN5` |

`libpksave` wraps all generation-specific code and data in `#ifdef PKSAVE_GENx` guards.

---

## libpksave

**Language:** C99  
**License:** MIT  
**Dependencies:** none (stdint.h, stddef.h, string.h only)  
**Format source:** pret decomp projects (primary), PKHeX source (secondary, edge cases only)

### Game Variant Enum

```c
typedef enum {
    PKSAVE_GAME_UNKNOWN = 0,
    PKSAVE_GAME_RB,        // Red, Blue
    PKSAVE_GAME_YELLOW,
    PKSAVE_GAME_GS,        // Gold, Silver
    PKSAVE_GAME_CRYSTAL,
    PKSAVE_GAME_RS,        // Ruby, Sapphire
    PKSAVE_GAME_EMERALD,
    PKSAVE_GAME_FRLG,      // FireRed, LeafGreen
    PKSAVE_GAME_DP,        // Diamond, Pearl
    PKSAVE_GAME_PLATINUM,
    PKSAVE_GAME_HGSS,      // HeartGold, SoulSilver
    PKSAVE_GAME_BW,        // Black, White
    PKSAVE_GAME_BW2,       // Black 2, White 2
} pksave_game_t;
```

### Detection

```c
pksave_game_t pksave_detect(const uint8_t *sram, size_t len);
```

Detection signals per variant:

| Variant | Signal |
|---|---|
| `GEN1_RB` | 32KB, Gen I checksum structure, no Pikachu block |
| `GEN1_YELLOW` | 32KB, Pikachu friendship block present |
| `GEN2_GS` | 32KB, Gen II structure, Crystal flag unset |
| `GEN2_CRYSTAL` | 32KB, Crystal flag byte set |
| `GEN3_RS` / `EMERALD` / `FRLG` | 128KB, game code in save block header |
| `GEN4_DP` / `PLATINUM` / `HGSS` | 512KB, save footer block ID |
| `GEN5_BW` / `BW2` | 512KB, save footer block ID |

Returns `PKSAVE_GAME_UNKNOWN` if no variant matches.

### Core Data Types

```c
typedef struct {
    char     name[13];     // null-terminated, game charset decoded to ASCII
    uint16_t tid;
    uint16_t sid;
    uint32_t money;
    uint8_t  badges;       // bitfield; meaning is generation-specific
} pksave_trainer_t;

typedef struct {
    uint16_t species;
    uint16_t held_item;
    uint8_t  level;
    uint8_t  nature;       // 0-24; absent in Gen I/II (set to 0)
    uint8_t  ability;      // ability slot 0/1; absent in Gen I/II
    uint8_t  is_shiny;
    uint8_t  move_ids[4];
    uint8_t  move_pp[4];
    uint16_t evs[6];       // HP/Atk/Def/SpA/SpD/Spe; Gen I/II use stat exp
    uint8_t  ivs[6];       // 0-31; Gen I/II use 0-15 DV mapped to 0-31
    char     nickname[13];
    char     ot_name[13];
    uint16_t stat[6];      // calculated, read from save (not recomputed by lib)
} pksave_pokemon_t;

typedef struct {
    uint16_t item_id;
    uint16_t quantity;
} pksave_item_t;

typedef enum {
    PKSAVE_POCKET_ITEMS = 0,
    PKSAVE_POCKET_BALLS,
    PKSAVE_POCKET_TM_HM,
    PKSAVE_POCKET_KEY,
    PKSAVE_POCKET_BERRIES,
    PKSAVE_POCKET_COUNT,
} pksave_pocket_t;
```

Not all pockets exist in all generations. `pksave_get_pocket` returns 0 count for pockets absent in the detected game.

### Session API

```c
typedef struct pksave_save pksave_save_t;

pksave_save_t *pksave_open(uint8_t *sram, size_t len, pksave_game_t game);
void           pksave_close(pksave_save_t *save);
int            pksave_flush(pksave_save_t *save); // recalculates all checksums
```

`pksave_open` takes a pointer directly into SRAM. `pksave_flush` writes changes back into that same buffer and recalculates checksums. The caller is responsible for the buffer lifetime.

### Trainer API

```c
int pksave_get_trainer(pksave_save_t *save, pksave_trainer_t *out);
int pksave_set_trainer(pksave_save_t *save, const pksave_trainer_t *in);
```

### Party API

```c
int pksave_get_party(pksave_save_t *save, pksave_pokemon_t out[6], uint8_t *count);
int pksave_set_party_slot(pksave_save_t *save, uint8_t slot, const pksave_pokemon_t *mon);
```

### Box API

```c
int pksave_get_box_count(pksave_save_t *save);
int pksave_get_box(pksave_save_t *save, uint8_t box, pksave_pokemon_t out[30], uint8_t *count);
int pksave_set_box_slot(pksave_save_t *save, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon);
```

### Bag API

```c
int pksave_get_pocket(pksave_save_t *save, pksave_pocket_t pocket,
                      pksave_item_t *out, uint8_t *count);
int pksave_set_item(pksave_save_t *save, pksave_pocket_t pocket,
                    uint8_t slot, const pksave_item_t *item);
```

### Name Lookup API

```c
const char *pksave_species_name(pksave_game_t game, uint16_t species_id);
const char *pksave_move_name(pksave_game_t game, uint16_t move_id);
const char *pksave_item_name(pksave_game_t game, uint16_t item_id);
const char *pksave_ability_name(pksave_game_t game, uint8_t ability_id);
const char *pksave_nature_name(uint8_t nature_id);
```

All functions return `"???"` for unknown IDs. Return values are pointers into static read-only storage — do not free.

### Error Codes

```c
#define PKSAVE_OK            0
#define PKSAVE_ERR_NULL     -1   // null pointer argument
#define PKSAVE_ERR_RANGE    -2   // slot/box/pocket index out of range
#define PKSAVE_ERR_GAME     -3   // operation not supported for this game variant
#define PKSAVE_ERR_CORRUPT  -4   // checksum validation failed on open
```

---

## Testing

**Framework:** Unity (single-file, embedded C test framework)  
**Container:** `docker/test` — standard gcc + Unity, all five `-DPKSAVE_GENx` flags  
**Fixtures:** one real or synthetically-generated `.sav` per format variant in `tests/fixtures/`

Each test file covers one generation:
- Load fixture → assert known trainer name/money/badges
- Read party slot 0 → assert species/level/moves
- Mutate a field → `pksave_flush` → reload → assert mutated value persists
- Assert checksum is valid after flush

---

## Frontend Architecture

All three frontends follow the same logical structure:

1. **Boot**: call `pksave_detect(sram, len)` — show error screen if `UNKNOWN`
2. **Main menu**: navigate Trainer / Party / Bag / Boxes
3. **Edit screen**: load data via libpksave, render with platform UI, write back on confirm
4. **Commit**: call `pksave_flush` before returning to main menu (not on every keystroke)

Frontends own: rendering, input handling, font/sprite assets, screen layout.  
Frontends do not own: save format knowledge, checksums, struct layouts, name strings.

### GBC Frontend (gbdk-2020, C)

- Tile-based list menus
- GBC enhanced palettes (not DMG monochrome-only)
- Navigation: D-pad scrolls, A confirms, B cancels/back
- Value editing: left/right increments/decrements numeric fields; species/move/item cycle by name using lookup tables
- No sprites; Pokeball and category icon tiles only

### GBA Frontend (butano, C++)

- Two-panel layout: list panel (left ~120px), detail panel (right ~120px)
- Party screen renders 64×64 front sprite from embedded sprite sheet
- L/R cycles top-level sections
- `libpksave` linked as a static C library; butano C++ code calls it via `extern "C"`

### NDS Frontend (BlocksDS, C)

- Bottom screen: interactive list with large touch targets (minimum 24px tall)
- Top screen: read-only summary (sprite, nickname, species, level, nature)
- Stylus tap selects; D-pad mirrors all touch navigation
- Both screens updated on selection change

---

## Format Research Notes

Primary sources (in order of authority):
1. **pret decomp projects** — authoritative struct layouts and offsets:
   - https://github.com/pret/pokered (Gen I)
   - https://github.com/pret/pokecrystal (Gen II)
   - https://github.com/pret/pokeemerald, pokeruby, pokefirered (Gen III)
   - https://github.com/pret/pokeplatinum, pokegold (Gen IV)
   - Gen V decomps (in progress as of 2025 — supplement with PKHeX)
2. **PKHeX** (https://github.com/kwsch/PKHeX) — reference for checksum edge cases and Gen V structures not yet fully decomped; MIT-incompatible (GPL v3), so reference only, no copy-paste

All libpksave implementations must be written from scratch using the above as documentation, not as a translation of existing code.
