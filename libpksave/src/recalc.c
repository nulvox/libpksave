/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * pksave_recalc_stats — recompute the stat[6] array from
 * species + level + nature + IVs + EVs.
 *
 * Gen I/II formula (pret/pokered CalcStats, pret/pokecrystal CalcPkmnStats):
 *   Bonus = floor(min(255, ceil(sqrt(StatExp))) / 4)
 *   HP    = floor(((Base + DV) * 2 + Bonus) * Level / 100) + Level + 10
 *   Other = floor(((Base + DV) * 2 + Bonus) * Level / 100) + 5
 *
 * Gen III formula:
 *   HP    = floor((2*Base + IV + EV/4) * Level / 100) + Level + 10
 *   Other = floor((2*Base + IV + EV/4) * Level / 100) + 5,
 *           then ×1.1 (raised by nature) or ×0.9 (lowered) and floor.
 */
#include "pksave_internal.h"

#if defined(PKSAVE_GEN1) || defined(PKSAVE_GEN2)

/* Matches GetSquareRoot in pret/pokered & pret/pokecrystal:
 * smallest b in [1, 255] with b*b >= stat_exp (0 when stat_exp == 0),
 * then divided by 4. */
static uint8_t gen12_stat_bonus(uint16_t stat_exp)
{
    if (stat_exp == 0) return 0;
    uint32_t b = 1;
    while (b < 255U && b * b < (uint32_t)stat_exp) {
        b++;
    }
    return (uint8_t)(b / 4U);
}

static uint16_t gen12_hp_stat(uint8_t base, uint8_t iv_scaled,
                              uint16_t stat_exp, uint8_t level)
{
    uint8_t dv = (uint8_t)((iv_scaled / 2U) & 0x0FU);
    uint8_t bonus = gen12_stat_bonus(stat_exp);
    uint32_t num = ((uint32_t)(base + dv) * 2U + bonus) * level;
    return (uint16_t)(num / 100U + level + 10U);
}

static uint16_t gen12_other_stat(uint8_t base, uint8_t iv_scaled,
                                 uint16_t stat_exp, uint8_t level)
{
    uint8_t dv = (uint8_t)((iv_scaled / 2U) & 0x0FU);
    uint8_t bonus = gen12_stat_bonus(stat_exp);
    uint32_t num = ((uint32_t)(base + dv) * 2U + bonus) * level;
    return (uint16_t)(num / 100U + 5U);
}

#endif /* PKSAVE_GEN1 || PKSAVE_GEN2 */

#ifdef PKSAVE_GEN1
#include "names/gen1_base_stats.h"

static const gen1_base_stats_t *gen1_base_stats_lookup(uint16_t species)
{
    if (species >= sizeof(gen1_base_stats) / sizeof(gen1_base_stats[0])) {
        return NULL;
    }
    const gen1_base_stats_t *bs = &gen1_base_stats[species];
    if (bs->hp == 0 && bs->atk == 0 && bs->def == 0 &&
        bs->spc == 0 && bs->spe == 0) {
        return NULL;
    }
    return bs;
}
#endif /* PKSAVE_GEN1 */

#ifdef PKSAVE_GEN2
#include "names/gen2_base_stats.h"

static const gen2_base_stats_t *gen2_base_stats_lookup(uint16_t species)
{
    if (species >= sizeof(gen2_base_stats) / sizeof(gen2_base_stats[0])) {
        return NULL;
    }
    const gen2_base_stats_t *bs = &gen2_base_stats[species];
    if (bs->hp == 0 && bs->atk == 0 && bs->def == 0 &&
        bs->spa == 0 && bs->spd == 0 && bs->spe == 0) {
        return NULL;
    }
    return bs;
}
#endif /* PKSAVE_GEN2 */

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

#endif /* PKSAVE_GEN3 */

void pksave_recalc_stats(pksave_pokemon_t *pkm, pksave_game_t game)
{
    if (pkm == NULL) return;

#ifdef PKSAVE_GEN1
    if (game == PKSAVE_GAME_RB || game == PKSAVE_GAME_YELLOW) {
        const gen1_base_stats_t *bs = gen1_base_stats_lookup(pkm->species);
        if (bs == NULL) {
            memset(pkm->stat, 0, sizeof(pkm->stat));
            return;
        }
        pkm->stat[0] = gen12_hp_stat(bs->hp, pkm->ivs[0], pkm->evs[0], pkm->level);
        pkm->stat[1] = gen12_other_stat(bs->atk, pkm->ivs[1], pkm->evs[1], pkm->level);
        pkm->stat[2] = gen12_other_stat(bs->def, pkm->ivs[2], pkm->evs[2], pkm->level);
        pkm->stat[3] = pkm->stat[4] =
            gen12_other_stat(bs->spc, pkm->ivs[3], pkm->evs[3], pkm->level);
        pkm->stat[5] = gen12_other_stat(bs->spe, pkm->ivs[5], pkm->evs[5], pkm->level);
        return;
    }
#endif

#ifdef PKSAVE_GEN2
    if (game == PKSAVE_GAME_GS || game == PKSAVE_GAME_CRYSTAL) {
        const gen2_base_stats_t *bs = gen2_base_stats_lookup(pkm->species);
        if (bs == NULL) {
            memset(pkm->stat, 0, sizeof(pkm->stat));
            return;
        }
        pkm->stat[0] = gen12_hp_stat(bs->hp, pkm->ivs[0], pkm->evs[0], pkm->level);
        pkm->stat[1] = gen12_other_stat(bs->atk, pkm->ivs[1], pkm->evs[1], pkm->level);
        pkm->stat[2] = gen12_other_stat(bs->def, pkm->ivs[2], pkm->evs[2], pkm->level);
        pkm->stat[3] = gen12_other_stat(bs->spa, pkm->ivs[3], pkm->evs[3], pkm->level);
        pkm->stat[4] = gen12_other_stat(bs->spd, pkm->ivs[4], pkm->evs[4], pkm->level);
        pkm->stat[5] = gen12_other_stat(bs->spe, pkm->ivs[5], pkm->evs[5], pkm->level);
        return;
    }
#endif

#ifdef PKSAVE_GEN3
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
#endif

    (void)game;
    memset(pkm->stat, 0, sizeof(pkm->stat));
}
