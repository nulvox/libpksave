/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * Session API: pksave_open / pksave_close / pksave_flush and all public
 * Trainer/Party/Box/Bag accessors.  Each function dispatches to the
 * generation-specific implementation via the PKSAVE_GENx guards.
 *
 * TODO(3): fill in each dispatch arm as each gen is implemented.
 */
#include "pksave_internal.h"

/* One static slot — no malloc; caller owns the buffer. */
static struct pksave_save g_save;

#ifdef __SDCC
uint8_t *pksave_sram_at(uint8_t *sram, uint32_t off)
{
    *(volatile uint8_t *)0x4000U = (uint8_t)(off >> 13);
    return sram + (uint16_t)(off & 0x1FFFU);
}

const uint8_t *pksave_sram_cat(const uint8_t *sram, uint32_t off)
{
    *(volatile uint8_t *)0x4000U = (uint8_t)(off >> 13);
    return sram + (uint16_t)(off & 0x1FFFU);
}

void pksave_sram_copy(uint8_t *sram, uint32_t dst_off,
                      uint32_t src_off, uint16_t len)
{
    uint8_t buf[32];
    uint16_t pos = 0;
    while (pos < len) {
        uint16_t chunk = (uint16_t)(len - pos);
        if (chunk > (uint16_t)sizeof(buf)) chunk = (uint16_t)sizeof(buf);
        memcpy(buf, pksave_sram_cat(sram, src_off + pos), chunk);
        memcpy(pksave_sram_at(sram, dst_off + pos), buf, chunk);
        pos = (uint16_t)(pos + chunk);
    }
}
#endif

void pksave_pokerus_decode(uint8_t byte, pksave_pokerus_t *out)
{
    out->strain = (uint8_t)(byte >> 4);
    out->days   = (uint8_t)(byte & 0x0F);
    if (out->strain == 0)     out->status = PKSAVE_PKRS_NEVER;
    else if (out->days > 0)   out->status = PKSAVE_PKRS_ACTIVE;
    else                      out->status = PKSAVE_PKRS_CURED;
}

uint8_t pksave_pokerus_encode(uint8_t strain, uint8_t days)
{
    return (uint8_t)(((strain & 0x0F) << 4) | (days & 0x0F));
}

#if defined(PKSAVE_GEN1) || defined(PKSAVE_GEN2) || defined(PKSAVE_GEN3)
uint32_t pksave_xp_for_level(uint8_t rate, uint8_t lvl)
{
    if (lvl <= 1) return 0;
    uint32_t n  = (uint32_t)lvl;
    uint32_t n2 = n * n;
    uint32_t n3 = n2 * n;
    switch (rate) {
    case 0: /* MEDIUM_FAST */
        return n3;
    case 1: /* ERRATIC */
        if (n <= 50UL) return ((100UL - n) * n3) /  50UL;
        if (n <= 68UL) return ((150UL - n) * n3) / 100UL;
        if (n <= 98UL) return (((1911UL - 10UL * n) / 3UL) * n3) / 500UL;
        return ((160UL - n) * n3) / 100UL;
    case 2: /* FLUCTUATING */
        if (n <= 15UL) return ((((n + 1UL) / 3UL) + 24UL) * n3) /  50UL;
        if (n <= 36UL) return ((n + 14UL)                 * n3) /  50UL;
        return (((n / 2UL) + 32UL)                        * n3) /  50UL;
    case 3: /* MEDIUM_SLOW */
        return (6UL * n3) / 5UL + 100UL * n - 15UL * n2 - 140UL;
    case 4: /* FAST */
        return (4UL * n3) / 5UL;
    case 5: /* SLOW */
        return (5UL * n3) / 4UL;
    default:
        return n3;
    }
}

uint8_t pksave_level_from_xp(uint8_t rate, uint32_t xp)
{
    uint8_t lvl = 100;
    while (lvl > 1 && pksave_xp_for_level(rate, lvl) > xp) {
        --lvl;
    }
    return lvl;
}
#endif

pksave_save_t *pksave_open(uint8_t *sram, size_t len, pksave_game_t game)
{
    if (!sram) return NULL;
    g_save.sram     = sram;
    g_save.sram_len = len;
    g_save.game     = game;
    return &g_save;
}

void pksave_close(pksave_save_t *save)
{
    if (!save) return;
    save->sram = NULL;
}

int pksave_flush(pksave_save_t *save)
{
    if (!save || !save->sram) return PKSAVE_ERR_NULL;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_flush(save);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_flush(save);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_flush(save);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_flush(save);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_flush(save);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

/* ── Trainer ─────────────────────────────────────────────────────────────── */

int pksave_get_trainer(pksave_save_t *save, pksave_trainer_t *out)
{
    if (!save || !out) return PKSAVE_ERR_NULL;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_get_trainer(save, out);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_get_trainer(save, out);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_get_trainer(save, out);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_get_trainer(save, out);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_get_trainer(save, out);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

int pksave_set_trainer(pksave_save_t *save, const pksave_trainer_t *in)
{
    if (!save || !in) return PKSAVE_ERR_NULL;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_set_trainer(save, in);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_set_trainer(save, in);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_set_trainer(save, in);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_set_trainer(save, in);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_set_trainer(save, in);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

/* ── Party ───────────────────────────────────────────────────────────────── */

int pksave_get_party(pksave_save_t *save, pksave_pokemon_t out[6], uint8_t *count)
{
    if (!save || !out || !count) return PKSAVE_ERR_NULL;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_get_party(save, out, count);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_get_party(save, out, count);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_get_party(save, out, count);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_get_party(save, out, count);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_get_party(save, out, count);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

int pksave_set_party_slot(pksave_save_t *save, uint8_t slot,
                          const pksave_pokemon_t *mon)
{
    if (!save || !mon) return PKSAVE_ERR_NULL;
    if (slot >= 6)    return PKSAVE_ERR_RANGE;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_set_party_slot(save, slot, mon);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_set_party_slot(save, slot, mon);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_set_party_slot(save, slot, mon);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_set_party_slot(save, slot, mon);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_set_party_slot(save, slot, mon);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

/* ── Box ─────────────────────────────────────────────────────────────────── */

int pksave_get_box_count(pksave_save_t *save)
{
    if (!save) return PKSAVE_ERR_NULL;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return 12;
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return 14;
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return 14;
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return 18;
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return 24;
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

int pksave_get_box(pksave_save_t *save, uint8_t box,
                   pksave_pokemon_t out[30], uint8_t *count)
{
    if (!save || !out || !count) return PKSAVE_ERR_NULL;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_get_box(save, box, out, count);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_get_box(save, box, out, count);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_get_box(save, box, out, count);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_get_box(save, box, out, count);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_get_box(save, box, out, count);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

int pksave_set_box_slot(pksave_save_t *save, uint8_t box, uint8_t slot,
                        const pksave_pokemon_t *mon)
{
    if (!save || !mon) return PKSAVE_ERR_NULL;
    if (slot >= 30)    return PKSAVE_ERR_RANGE;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_set_box_slot(save, box, slot, mon);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_set_box_slot(save, box, slot, mon);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_set_box_slot(save, box, slot, mon);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_set_box_slot(save, box, slot, mon);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_set_box_slot(save, box, slot, mon);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

/* ── Pokerus ─────────────────────────────────────────────────────────────── */

int pksave_get_party_pokerus(pksave_save_t *save, uint8_t slot,
                              pksave_pokerus_t *out)
{
    if (!save || !out) return PKSAVE_ERR_NULL;
    if (slot >= 6)     return PKSAVE_ERR_RANGE;
    switch (save->game) {
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_get_party_pokerus(save, slot, out);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_get_party_pokerus(save, slot, out);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_get_party_pokerus(save, slot, out);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_get_party_pokerus(save, slot, out);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

int pksave_set_party_pokerus(pksave_save_t *save, uint8_t slot,
                              uint8_t strain, uint8_t days)
{
    if (!save)         return PKSAVE_ERR_NULL;
    if (slot >= 6)     return PKSAVE_ERR_RANGE;
    if (strain > 0x0F || days > 0x0F) return PKSAVE_ERR_RANGE;
    switch (save->game) {
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_set_party_pokerus(save, slot, strain, days);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_set_party_pokerus(save, slot, strain, days);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_set_party_pokerus(save, slot, strain, days);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_set_party_pokerus(save, slot, strain, days);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

int pksave_get_box_pokerus(pksave_save_t *save, uint8_t box, uint8_t slot,
                            pksave_pokerus_t *out)
{
    if (!save || !out) return PKSAVE_ERR_NULL;
    if (slot >= 30)    return PKSAVE_ERR_RANGE;
    switch (save->game) {
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_get_box_pokerus(save, box, slot, out);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_get_box_pokerus(save, box, slot, out);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_get_box_pokerus(save, box, slot, out);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_get_box_pokerus(save, box, slot, out);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

int pksave_set_box_pokerus(pksave_save_t *save, uint8_t box, uint8_t slot,
                            uint8_t strain, uint8_t days)
{
    if (!save)         return PKSAVE_ERR_NULL;
    if (slot >= 30)    return PKSAVE_ERR_RANGE;
    if (strain > 0x0F || days > 0x0F) return PKSAVE_ERR_RANGE;
    switch (save->game) {
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_set_box_pokerus(save, box, slot, strain, days);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_set_box_pokerus(save, box, slot, strain, days);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_set_box_pokerus(save, box, slot, strain, days);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_set_box_pokerus(save, box, slot, strain, days);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

/* ── Bag ─────────────────────────────────────────────────────────────────── */

int pksave_get_pocket(pksave_save_t *save, pksave_pocket_t pocket,
                      pksave_item_t *out, uint8_t *count)
{
    if (!save || !out || !count) return PKSAVE_ERR_NULL;
    if (pocket >= PKSAVE_POCKET_COUNT) return PKSAVE_ERR_RANGE;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_get_pocket(save, pocket, out, count);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_get_pocket(save, pocket, out, count);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_get_pocket(save, pocket, out, count);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_get_pocket(save, pocket, out, count);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_get_pocket(save, pocket, out, count);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}

int pksave_set_item(pksave_save_t *save, pksave_pocket_t pocket,
                    uint8_t slot, const pksave_item_t *item)
{
    if (!save || !item) return PKSAVE_ERR_NULL;
    if (pocket >= PKSAVE_POCKET_COUNT) return PKSAVE_ERR_RANGE;
    switch (save->game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:    return gen1_set_item(save, pocket, slot, item);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:   return gen2_set_item(save, pocket, slot, item);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:      return gen3_set_item(save, pocket, slot, item);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:      return gen4_set_item(save, pocket, slot, item);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:       return gen5_set_item(save, pocket, slot, item);
#endif
        default: return PKSAVE_ERR_GAME;
    }
}
