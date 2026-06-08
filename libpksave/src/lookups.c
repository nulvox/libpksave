/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Per-species and per-move lookup helpers for Gen 3.  Both APIs are
 * thin reads from parallel tables in names/gen3_*.h, switched on
 * pksave_game_t so non-Gen-3 games return a sentinel (zeroed struct
 * for abilities, 0 for PP).
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN3
#include "names/gen3_base_stats.h"
#include "names/gen3_move_pp.h"
#include "names/gen3_item_max_stack.h"
#include "names/gen3_item_pocket.h"
#endif

pksave_species_abilities_t pksave_species_abilities(pksave_game_t game,
                                                    uint16_t species_id)
{
    pksave_species_abilities_t out = { 0, 0 };
#ifdef PKSAVE_GEN3
    if (game == PKSAVE_GAME_RS ||
        game == PKSAVE_GAME_EMERALD ||
        game == PKSAVE_GAME_FRLG) {
        if (species_id < sizeof(gen3_base_stats) / sizeof(gen3_base_stats[0])) {
            out.slot0 = gen3_base_stats[species_id].ability1;
            out.slot1 = gen3_base_stats[species_id].ability2;
        }
    }
#else
    (void)game;
    (void)species_id;
#endif
    return out;
}

uint8_t pksave_move_base_pp(pksave_game_t game, uint16_t move_id)
{
#ifdef PKSAVE_GEN3
    if (game == PKSAVE_GAME_RS ||
        game == PKSAVE_GAME_EMERALD ||
        game == PKSAVE_GAME_FRLG) {
        if (move_id < sizeof(gen3_move_pp) / sizeof(gen3_move_pp[0])) {
            return gen3_move_pp[move_id];
        }
    }
#else
    (void)game;
    (void)move_id;
#endif
    return 0;
}

uint16_t pksave_item_max_stack(pksave_game_t game, uint16_t item_id)
{
#ifdef PKSAVE_GEN3
    if (game == PKSAVE_GAME_RS ||
        game == PKSAVE_GAME_EMERALD ||
        game == PKSAVE_GAME_FRLG) {
        if (item_id < sizeof(gen3_item_max_stack) / sizeof(gen3_item_max_stack[0])) {
            return gen3_item_max_stack[item_id];
        }
    }
#else
    (void)game;
    (void)item_id;
#endif
    return 0;
}

pksave_pocket_t pksave_item_pocket(pksave_game_t game, uint16_t item_id)
{
#ifdef PKSAVE_GEN3
    if (game == PKSAVE_GAME_RS ||
        game == PKSAVE_GAME_EMERALD ||
        game == PKSAVE_GAME_FRLG) {
        if (item_id < sizeof(gen3_item_pocket) / sizeof(gen3_item_pocket[0])) {
            return (pksave_pocket_t)gen3_item_pocket[item_id];
        }
    }
#else
    (void)game;
    (void)item_id;
#endif
    return PKSAVE_POCKET_NONE;
}
