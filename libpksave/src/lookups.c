/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * Per-species, per-move, and per-item lookup helpers for Gen I–III.
 * Each table is guarded by its PKSAVE_GENx flag so unused tables are
 * stripped at compile time.
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN1
#include "names/gen1_move_pp.h"
#include "names/gen1_item_max_stack.h"
#include "names/gen1_item_pocket.h"
#endif

#ifdef PKSAVE_GEN2
#include "names/gen2_move_pp.h"
#include "names/gen2_item_max_stack.h"
#include "names/gen2_item_pocket.h"
#endif

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
    switch (game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:
            if (move_id < sizeof(gen1_move_pp) / sizeof(gen1_move_pp[0])) {
                return gen1_move_pp[move_id];
            }
            return 0;
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:
            if (move_id < sizeof(gen2_move_pp) / sizeof(gen2_move_pp[0])) {
                return gen2_move_pp[move_id];
            }
            return 0;
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:
            if (move_id < sizeof(gen3_move_pp) / sizeof(gen3_move_pp[0])) {
                return gen3_move_pp[move_id];
            }
            return 0;
#endif
        default:
            (void)move_id;
            return 0;
    }
}

uint16_t pksave_item_max_stack(pksave_game_t game, uint16_t item_id)
{
    switch (game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:
            if (item_id < sizeof(gen1_item_max_stack) / sizeof(gen1_item_max_stack[0])) {
                return gen1_item_max_stack[item_id];
            }
            return 0;
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:
            if (item_id < sizeof(gen2_item_max_stack) / sizeof(gen2_item_max_stack[0])) {
                return gen2_item_max_stack[item_id];
            }
            return 0;
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:
            if (item_id < sizeof(gen3_item_max_stack) / sizeof(gen3_item_max_stack[0])) {
                return gen3_item_max_stack[item_id];
            }
            return 0;
#endif
        default:
            (void)item_id;
            return 0;
    }
}

pksave_pocket_t pksave_item_pocket(pksave_game_t game, uint16_t item_id)
{
    switch (game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:
            if (item_id < sizeof(gen1_item_pocket) / sizeof(gen1_item_pocket[0])) {
                return (pksave_pocket_t)gen1_item_pocket[item_id];
            }
            return PKSAVE_POCKET_NONE;
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:
            if (item_id < sizeof(gen2_item_pocket) / sizeof(gen2_item_pocket[0])) {
                return (pksave_pocket_t)gen2_item_pocket[item_id];
            }
            return PKSAVE_POCKET_NONE;
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:
            if (item_id < sizeof(gen3_item_pocket) / sizeof(gen3_item_pocket[0])) {
                return (pksave_pocket_t)gen3_item_pocket[item_id];
            }
            return PKSAVE_POCKET_NONE;
#endif
        default:
            (void)item_id;
            return PKSAVE_POCKET_NONE;
    }
}
