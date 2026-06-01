# PRD: Multi-Platform Pokemon Save Editor

## Overview

A homebrew save editor targeting GBC, GBA, and NDS platforms that runs inside emulators. Users symlink their Pokemon game's `.sav` file to the editor ROM's battery-backed SRAM slot. The editor reads, modifies, and writes the save in-place. No special hardware, no file pickers, no cross-compiler setup required to use.

## Target Users

ROM hacking hobbyists and casual Pokemon fans running games in emulators who want to inspect or modify their save data without leaving the emulator or using a separate desktop application.

## Platforms and Game Coverage

| Editor ROM | SDK | Games |
|---|---|---|
| `pksave-gbc.gbc` | gbdk-2020 | Red, Blue, Yellow, Gold, Silver, Crystal |
| `pksave-gba.gba` | butano (devkitPro) | Ruby, Sapphire, FireRed, LeafGreen, Emerald |
| `pksave-nds.nds` | BlocksDS | Diamond, Pearl, Platinum, HeartGold, SoulSilver, Black, White, Black 2, White 2 |

## Core Features (v1)

All three ROMs expose the same logical feature set:

### Trainer Profile
- Name, Trainer ID, Secret ID
- Money
- Badges / gym progress indicators

### Party Pokemon
- Species, nickname, OT name
- Level, nature, ability, held item
- Moves (4 slots, move name + PP)
- EVs and IVs (all 6 stats)
- Shininess flag
- Calculated stats display (read-only)

### PC Boxes
- Full read/write access to all boxes
- Navigate boxes and slots
- Move Pokemon between slots
- Party and Box editing share the same Pokemon editing component

### Bag Items
- All item pockets (Items, Balls, TMs/HMs, Key Items, Berries — present per-game)
- Edit quantity per slot
- Human-readable item names

## Non-Goals (v1)

- Pokedex seen/caught flags
- Contest stats, ribbons, friendship
- Secret base / underground / Pokeathalon / C-Gear data
- Game-specific event flags
- Hardware support (flashcarts, link cable, cart-to-cart)
- Desktop application wrapper
- ROM patches or code injection

## UX Per Platform

### GBC (160×144)
Tile-based list menus with GBC palette color enhancement. Top-level navigation: Trainer → Party → Bag → Boxes. All editing via D-pad + A/B. Field values edited with left/right. No Pokemon sprites — icon tiles only.

### GBA (240×160)
Two-panel layout: scrollable list left, detail panel right. Party screen shows 64×64 front sprite. L/R shoulder buttons cycle top-level sections. D-pad navigates lists, A confirms, B cancels.

### NDS (2 × 256×192)
Bottom screen: interactive list, large touch targets, stylus-navigable. Top screen: read-only summary of selected Pokemon or item (sprite, stats, name). D-pad mirrors touch navigation for stylus-free use.

## How Save Access Works

The editor ROM has no filesystem access and makes no attempt to discover save files. The user is responsible for symlinking the Pokemon game's `.sav` to the path the emulator uses for the editor ROM's SRAM. The editor reads from and writes to its own SRAM — which is the Pokemon save. `libpksave` recalculates all checksums on write so the save remains valid when loaded back into the original game.

## Game Detection

The editor auto-detects the save format from SRAM content on startup. Detection targets are format variants, not individual game titles (Red and Blue share one format; Gold and Silver share one format). No user prompt is required. If detection fails, an error screen is shown.

## Success Criteria

- All three ROMs boot in their respective emulators (mGBA, melonDS, VisualBoyAdvance-M) with a symlinked `.sav`
- Trainer profile round-trips correctly (edit name/money → load in original game → value persists)
- Party Pokemon round-trip correctly (edit species/moves → load in original game → Pokemon matches)
- Bag item quantities round-trip correctly
- PC Box read/write round-trips correctly
- Checksums are valid after every write (no "save data corrupted" on load)
- All 12 format variants detected correctly from fixture saves
