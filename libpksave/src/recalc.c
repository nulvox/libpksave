/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * pksave_recalc_stats — recompute the stat[6] array from
 * species + level + nature + IVs + EVs using the official Gen III formula.
 *
 *   HP    = floor((2*Base + IV + EV/4) * Level / 100) + Level + 10
 *   Other = floor((2*Base + IV + EV/4) * Level / 100) + 5,
 *           then ×1.1 (raised by nature) or ×0.9 (lowered) and floor.
 *
 * Base stats are looked up per generation.  Gen III tables for species
 * 1-411 live in names/gen3_base_stats.h (the Hoenn + Kanto/Johto roster).
 * Currently a stub — replaced with the full table once the generator
 * subagent lands names/gen3_base_stats.h.
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN3

#include "names/gen3_base_stats.h"

static const gen3_base_stats_t *gen3_base_stats_lookup(uint16_t species)
{
    if (species >= sizeof(gen3_base_stats) / sizeof(gen3_base_stats[0])) {
        return NULL;
    }
    const gen3_base_stats_t *bs = &gen3_base_stats[species];
    /* Index 0, the OLD_UNOWN placeholders (252-276), and Egg (412) are
     * all-zero rows.  Treat them as "no stats" so recalc zeros the output
     * rather than producing nonsense for a placeholder species. */
    if (bs->hp == 0 && bs->atk == 0 && bs->def == 0 &&
        bs->spa == 0 && bs->spd == 0 && bs->spe == 0) {
        return NULL;
    }
    return bs;
}

static uint16_t gen3_hp_stat(uint8_t base, uint8_t iv, uint16_t ev, uint8_t level)
{
    /* Integer arithmetic (no FP needed): (2B + IV + EV/4) * L / 100 + L + 10. */
    uint32_t raw = (uint32_t)(2U * base + iv + ev / 4U);
    uint32_t scaled = (raw * level) / 100U;
    return (uint16_t)(scaled + level + 10U);
}

static uint16_t gen3_other_stat(uint8_t base, uint8_t iv, uint16_t ev, uint8_t level)
{
    /* (2B + IV + EV/4) * L / 100 + 5, before nature ±10% mod. */
    uint32_t raw = (uint32_t)(2U * base + iv + ev / 4U);
    uint32_t scaled = (raw * level) / 100U;
    return (uint16_t)(scaled + 5U);
}

/* Nature decomposes as raised = id / 5, lowered = id % 5 over Gen 3's
 * canonical order (Atk, Def, Spe, SpA, SpD).  This table maps that
 * canonical index to the struct stat[] index (HP at 0, Spe at 5). */
static const uint8_t gen3_nature_to_struct[5] = { 1, 2, 5, 3, 4 };

static uint16_t gen3_apply_nature(uint16_t stat, uint8_t nature, uint8_t struct_idx)
{
    uint8_t raised_canon = (uint8_t)(nature / 5U);
    uint8_t lowered_canon = (uint8_t)(nature % 5U);
    if (raised_canon == lowered_canon) return stat;     /* neutral nature */
    if (struct_idx == gen3_nature_to_struct[raised_canon]) {
        return (uint16_t)(((uint32_t)stat * 110U) / 100U);
    }
    if (struct_idx == gen3_nature_to_struct[lowered_canon]) {
        return (uint16_t)(((uint32_t)stat * 90U) / 100U);
    }
    return stat;
}

void pksave_recalc_stats(pksave_pokemon_t *pkm, pksave_game_t game)
{
    if (pkm == NULL) return;

    if (game == PKSAVE_GAME_RS ||
        game == PKSAVE_GAME_EMERALD ||
        game == PKSAVE_GAME_FRLG) {
        const gen3_base_stats_t *bs = gen3_base_stats_lookup(pkm->species);
        if (bs == NULL) {
            memset(pkm->stat, 0, sizeof(pkm->stat));
            return;
        }
        const uint8_t bases[6] = { bs->hp, bs->atk, bs->def,
                                   bs->spa, bs->spd, bs->spe };
        pkm->stat[0] = gen3_hp_stat(bases[0], pkm->ivs[0],
                                    pkm->evs[0], pkm->level);
        for (int i = 1; i < 6; i++) {
            uint16_t raw = gen3_other_stat(bases[i], pkm->ivs[i],
                                           pkm->evs[i], pkm->level);
            pkm->stat[i] = gen3_apply_nature(raw, pkm->nature, (uint8_t)i);
        }
        return;
    }

    memset(pkm->stat, 0, sizeof(pkm->stat));
}

#else  /* PKSAVE_GEN3 not defined */

void pksave_recalc_stats(pksave_pokemon_t *pkm, pksave_game_t game)
{
    (void)game;
    if (pkm) memset(pkm->stat, 0, sizeof(pkm->stat));
}

#endif
