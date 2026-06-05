/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 */
#ifndef PKSAVE_H
#define PKSAVE_H

#include <stddef.h>
#include <stdint.h>

/* ── Error codes ─────────────────────────────────────────────────────────── */
#define PKSAVE_OK           0
#define PKSAVE_ERR_NULL    -1   /* null pointer argument */
#define PKSAVE_ERR_RANGE   -2   /* slot/box/pocket index out of range */
#define PKSAVE_ERR_GAME    -3   /* operation not supported for this game variant */
#define PKSAVE_ERR_CORRUPT -4   /* checksum validation failed on open */

/* ── Game variant ────────────────────────────────────────────────────────── */
typedef enum {
    PKSAVE_GAME_UNKNOWN = 0,
    PKSAVE_GAME_RB,       /* Red, Blue              */
    PKSAVE_GAME_YELLOW,
    PKSAVE_GAME_GS,       /* Gold, Silver            */
    PKSAVE_GAME_CRYSTAL,
    PKSAVE_GAME_RS,       /* Ruby, Sapphire          */
    PKSAVE_GAME_EMERALD,
    PKSAVE_GAME_FRLG,     /* FireRed, LeafGreen      */
    PKSAVE_GAME_DP,       /* Diamond, Pearl          */
    PKSAVE_GAME_PLATINUM,
    PKSAVE_GAME_HGSS,     /* HeartGold, SoulSilver   */
    PKSAVE_GAME_BW,       /* Black, White            */
    PKSAVE_GAME_BW2,      /* Black 2, White 2        */
} pksave_game_t;

/* ── Bag pockets ─────────────────────────────────────────────────────────── */
typedef enum {
    PKSAVE_POCKET_ITEMS = 0,
    PKSAVE_POCKET_BALLS,
    PKSAVE_POCKET_TM_HM,
    PKSAVE_POCKET_KEY,
    PKSAVE_POCKET_BERRIES,
    PKSAVE_POCKET_COUNT,
} pksave_pocket_t;

/* ── Core data types ─────────────────────────────────────────────────────── */
typedef struct {
    char     name[13];    /* null-terminated, game charset decoded to ASCII */
    uint16_t tid;
    uint16_t sid;
    uint32_t money;
    uint8_t  badges;      /* bitfield; meaning is generation-specific */
} pksave_trainer_t;

typedef struct {
    uint16_t species;
    uint16_t held_item;
    uint8_t  level;
    uint8_t  nature;      /* 0-24; 0 for Gen I/II (absent) */
    uint8_t  ability;     /* slot 0/1; 0 for Gen I/II (absent) */
    uint8_t  is_shiny;
    uint8_t  move_ids[4];
    uint8_t  move_pp[4];
    uint16_t evs[6];      /* HP/Atk/Def/SpA/SpD/Spe; Gen I/II use stat exp */
    uint8_t  ivs[6];      /* 0-31; Gen I/II 0-15 DVs mapped to 0-31 */
    char     nickname[13];
    char     ot_name[13];
    uint16_t stat[6];     /* calculated stats, read from save */
} pksave_pokemon_t;

typedef struct {
    uint16_t item_id;
    uint16_t quantity;
} pksave_item_t;

/* ── Session API ─────────────────────────────────────────────────────────── */
typedef struct pksave_save pksave_save_t;

pksave_game_t  pksave_detect(const uint8_t *sram, size_t len);
pksave_save_t *pksave_open(uint8_t *sram, size_t len, pksave_game_t game);
void           pksave_close(pksave_save_t *save);
int            pksave_flush(pksave_save_t *save);

/* ── Trainer API ─────────────────────────────────────────────────────────── */
int pksave_get_trainer(pksave_save_t *save, pksave_trainer_t *out);
int pksave_set_trainer(pksave_save_t *save, const pksave_trainer_t *in);

/* ── Party API ───────────────────────────────────────────────────────────── */
int pksave_get_party(pksave_save_t *save, pksave_pokemon_t out[6], uint8_t *count);
int pksave_set_party_slot(pksave_save_t *save, uint8_t slot, const pksave_pokemon_t *mon);

/* ── Box API ─────────────────────────────────────────────────────────────── */
int pksave_get_box_count(pksave_save_t *save);
int pksave_get_box(pksave_save_t *save, uint8_t box,
                   pksave_pokemon_t out[30], uint8_t *count);
int pksave_set_box_slot(pksave_save_t *save, uint8_t box, uint8_t slot,
                        const pksave_pokemon_t *mon);

/* ── Bag API ─────────────────────────────────────────────────────────────── */
int pksave_get_pocket(pksave_save_t *save, pksave_pocket_t pocket,
                      pksave_item_t *out, uint8_t *count);
int pksave_set_item(pksave_save_t *save, pksave_pocket_t pocket,
                    uint8_t slot, const pksave_item_t *item);

/* ── Stat recalculation ──────────────────────────────────────────────────── */
/* Recompute pkm->stat[6] in-place from species, level, nature, IVs, and EVs.
 * Uses the official Gen III formula (HP and per-stat) with nature ±10%.
 * Caller is responsible for setting nature; pksave_get_party / get_box
 * populate it from PID on Gen III reads.  Unknown species → stats zeroed. */
void pksave_recalc_stats(pksave_pokemon_t *pkm, pksave_game_t game);

/* ── Pokerus API ─────────────────────────────────────────────────────────── */
/* Pokerus is a hidden status, present from Gen II onwards, that doubles the
 * EV gain a Pokémon receives from battles. It is stored as a single byte:
 *     high nibble = strain  (1-15; 0 = never infected)
 *     low nibble  = days remaining  (counts down to 0 then "cures")
 * States:
 *   strain == 0                 → NEVER (never infected, no EV boost)
 *   strain  > 0 && days > 0     → ACTIVE (infectious, EV boost active)
 *   strain  > 0 && days == 0    → CURED (no longer infectious; permanent EV boost)
 * Not supported on Gen I (the mechanic was introduced in Gen II).
 */
typedef enum {
    PKSAVE_PKRS_NEVER  = 0,
    PKSAVE_PKRS_ACTIVE = 1,
    PKSAVE_PKRS_CURED  = 2,
} pksave_pokerus_status_t;

typedef struct {
    uint8_t                  strain;   /* 0-15; 0 = never infected */
    uint8_t                  days;     /* 0-15; days of active infection remaining */
    pksave_pokerus_status_t  status;   /* derived from strain & days */
} pksave_pokerus_t;

int pksave_get_party_pokerus(pksave_save_t *save, uint8_t slot,
                             pksave_pokerus_t *out);
int pksave_set_party_pokerus(pksave_save_t *save, uint8_t slot,
                             uint8_t strain, uint8_t days);
int pksave_get_box_pokerus(pksave_save_t *save, uint8_t box, uint8_t slot,
                           pksave_pokerus_t *out);
int pksave_set_box_pokerus(pksave_save_t *save, uint8_t box, uint8_t slot,
                           uint8_t strain, uint8_t days);

/* ── Name lookup API ─────────────────────────────────────────────────────── */
/* All return pointers into static read-only storage; never free. */
/* Returns "???" for unknown IDs.                                 */
const char *pksave_species_name(pksave_game_t game, uint16_t species_id);
const char *pksave_move_name(pksave_game_t game, uint16_t move_id);
const char *pksave_item_name(pksave_game_t game, uint16_t item_id);
const char *pksave_ability_name(pksave_game_t game, uint8_t ability_id);
const char *pksave_nature_name(uint8_t nature_id);

/* ── Gen III in-game name-length limits (excluding null terminator) ─────── */
/* These are the character limits the Gen III name-entry screen enforces.
 * Per-gen equivalents (PKSAVE_GEN1_*, PKSAVE_GEN2_*, ...) will be added
 * when those frontends land. */
#define PKSAVE_GEN3_TRAINER_NAME_MAX  7
#define PKSAVE_GEN3_NICKNAME_MAX      10
#define PKSAVE_GEN3_OT_NAME_MAX       7

/* ── Per-game numeric limits ────────────────────────────────────────────── */
/* Maximum IV (or DV in Gen I/II) per stat.  Returns 0 for unknown game. */
uint8_t  pksave_iv_max(pksave_game_t game);

/* Maximum EV value per stat.  Gen III+: 252.  Gen I/II (stat exp): 65535.
 * Returns 0 for unknown game. */
uint16_t pksave_ev_max_per_stat(pksave_game_t game);

/* Maximum EV total summed across all six stats.  Gen III+: 510.  Gen I/II
 * has no total cap, so returns 0 as a sentinel meaning "no total cap".
 * Returns 0 for unknown game (same sentinel; consumers should consult
 * pksave_ev_max_per_stat to disambiguate). */
uint16_t pksave_ev_total_max(pksave_game_t game);

/* ── Test helper (test container only) ──────────────────────────────────── */
int pksave_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game);

#endif /* PKSAVE_H */
