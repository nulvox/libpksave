/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#ifndef PKSAVE_INTERNAL_H
#define PKSAVE_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "pksave.h"

/* Maximum size of any supported save file */
#define PKSAVE_SRAM_MAX (512 * 1024)

struct pksave_save {
    uint8_t       *sram;
    size_t         sram_len;
    pksave_game_t  game;
    /* Generation-specific state added here when implementing each gen */
};

/* Pokerus byte ↔ struct helpers shared by all gens.
 *   byte high nibble = strain (0-15), low nibble = days remaining (0-15).
 *   strain 0:                NEVER (no infection)
 *   strain >0 && days >0:    ACTIVE (infectious)
 *   strain >0 && days ==0:   CURED  (permanent EV boost, not infectious) */
static inline void pksave_pokerus_decode(uint8_t byte, pksave_pokerus_t *out)
{
    out->strain = (uint8_t)(byte >> 4);
    out->days   = (uint8_t)(byte & 0x0F);
    if (out->strain == 0)     out->status = PKSAVE_PKRS_NEVER;
    else if (out->days > 0)   out->status = PKSAVE_PKRS_ACTIVE;
    else                      out->status = PKSAVE_PKRS_CURED;
}

static inline uint8_t pksave_pokerus_encode(uint8_t strain, uint8_t days)
{
    return (uint8_t)(((strain & 0x0F) << 4) | (days & 0x0F));
}

/* Experience curve helpers shared by Gen I–III.
 * Growth rate enum:
 *   0 = MEDIUM_FAST   1 = ERRATIC     2 = FLUCTUATING
 *   3 = MEDIUM_SLOW   4 = FAST        5 = SLOW
 * (Gen I/II use only 0, 3, 4, 5; Gen III+ add 1 and 2.) */
static inline uint32_t pksave_xp_for_level(uint8_t rate, uint8_t lvl)
{
    if (lvl <= 1) return 0;
    uint32_t n  = (uint32_t)lvl;
    uint32_t n2 = n * n;
    uint32_t n3 = n2 * n;
    switch (rate) {
    case 0: /* MEDIUM_FAST */
        return n3;
    case 1: /* ERRATIC */
        if (n <= 50) return ((100u - n) * n3) /  50u;
        if (n <= 68) return ((150u - n) * n3) / 100u;
        if (n <= 98) return (((1911u - 10u * n) / 3u) * n3) / 500u;
        return ((160u - n) * n3) / 100u;
    case 2: /* FLUCTUATING */
        if (n <= 15) return ((((n + 1u) / 3u) + 24u) * n3) /  50u;
        if (n <= 36) return ((n + 14u)                * n3) /  50u;
        return (((n / 2u) + 32u)                      * n3) /  50u;
    case 3: /* MEDIUM_SLOW */
        return (6u * n3) / 5u + 100u * n - 15u * n2 - 140u;
    case 4: /* FAST */
        return (4u * n3) / 5u;
    case 5: /* SLOW */
        return (5u * n3) / 4u;
    default:
        return n3;
    }
}

static inline uint8_t pksave_level_from_xp(uint8_t rate, uint32_t xp)
{
    uint8_t lvl = 100;
    while (lvl > 1 && pksave_xp_for_level(rate, lvl) > xp) {
        --lvl;
    }
    return lvl;
}

/* Per-generation parse/serialize/checksum helpers declared here,
 * defined in each genN.c; guarded by PKSAVE_GENx at the call site. */

#ifdef PKSAVE_GEN1
int  gen1_get_trainer(pksave_save_t *s, pksave_trainer_t *out);
int  gen1_set_trainer(pksave_save_t *s, const pksave_trainer_t *in);
int  gen1_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count);
int  gen1_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon);
int  gen1_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt);
int  gen1_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon);
int  gen1_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt);
int  gen1_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot, const pksave_item_t *item);
int  gen1_flush(pksave_save_t *s);
int  gen1_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game);
#endif

#ifdef PKSAVE_GEN2
int  gen2_get_trainer(pksave_save_t *s, pksave_trainer_t *out);
int  gen2_set_trainer(pksave_save_t *s, const pksave_trainer_t *in);
int  gen2_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count);
int  gen2_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon);
int  gen2_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt);
int  gen2_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon);
int  gen2_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt);
int  gen2_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot, const pksave_item_t *item);
int  gen2_get_party_pokerus(pksave_save_t *s, uint8_t slot, pksave_pokerus_t *out);
int  gen2_set_party_pokerus(pksave_save_t *s, uint8_t slot, uint8_t strain, uint8_t days);
int  gen2_get_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot, pksave_pokerus_t *out);
int  gen2_set_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot, uint8_t strain, uint8_t days);
int  gen2_flush(pksave_save_t *s);
int  gen2_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game);
#endif

#ifdef PKSAVE_GEN3
int  gen3_get_trainer(pksave_save_t *s, pksave_trainer_t *out);
int  gen3_set_trainer(pksave_save_t *s, const pksave_trainer_t *in);
int  gen3_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count);
int  gen3_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon);
int  gen3_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt);
int  gen3_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon);
int  gen3_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt);
int  gen3_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot, const pksave_item_t *item);
int  gen3_get_party_pokerus(pksave_save_t *s, uint8_t slot, pksave_pokerus_t *out);
int  gen3_set_party_pokerus(pksave_save_t *s, uint8_t slot, uint8_t strain, uint8_t days);
int  gen3_get_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot, pksave_pokerus_t *out);
int  gen3_set_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot, uint8_t strain, uint8_t days);
int  gen3_flush(pksave_save_t *s);
int  gen3_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game);
#endif

#ifdef PKSAVE_GEN4
int  gen4_get_trainer(pksave_save_t *s, pksave_trainer_t *out);
int  gen4_set_trainer(pksave_save_t *s, const pksave_trainer_t *in);
int  gen4_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count);
int  gen4_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon);
int  gen4_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt);
int  gen4_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon);
int  gen4_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt);
int  gen4_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot, const pksave_item_t *item);
int  gen4_get_party_pokerus(pksave_save_t *s, uint8_t slot, pksave_pokerus_t *out);
int  gen4_set_party_pokerus(pksave_save_t *s, uint8_t slot, uint8_t strain, uint8_t days);
int  gen4_get_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot, pksave_pokerus_t *out);
int  gen4_set_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot, uint8_t strain, uint8_t days);
int  gen4_flush(pksave_save_t *s);
int  gen4_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game);
#endif

/* CRC-16-CCITT shared by Gen IV and Gen V validate/flush */
#if defined(PKSAVE_GEN4) || defined(PKSAVE_GEN5)
static inline uint16_t pksave_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
            else              crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}
#endif

#ifdef PKSAVE_GEN5
int  gen5_get_trainer(pksave_save_t *s, pksave_trainer_t *out);
int  gen5_set_trainer(pksave_save_t *s, const pksave_trainer_t *in);
int  gen5_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count);
int  gen5_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon);
int  gen5_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt);
int  gen5_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon);
int  gen5_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt);
int  gen5_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot, const pksave_item_t *item);
int  gen5_get_party_pokerus(pksave_save_t *s, uint8_t slot, pksave_pokerus_t *out);
int  gen5_set_party_pokerus(pksave_save_t *s, uint8_t slot, uint8_t strain, uint8_t days);
int  gen5_get_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot, pksave_pokerus_t *out);
int  gen5_set_box_pokerus(pksave_save_t *s, uint8_t box, uint8_t slot, uint8_t strain, uint8_t days);
int  gen5_flush(pksave_save_t *s);
int  gen5_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game);
#endif

#endif /* PKSAVE_INTERNAL_H */
