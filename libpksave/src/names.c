/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Name lookup tables are included here from names/genN_*.h.
 * Each header is guarded by its PKSAVE_GENx flag so unused tables are
 * stripped at compile time.
 *
 * TODO(3): populate each name header with the full name arrays.
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN1
#include "names/gen1_species.h"
#include "names/gen1_moves.h"
#include "names/gen1_items.h"
#endif

#ifdef PKSAVE_GEN2
#include "names/gen2_species.h"
#include "names/gen2_moves.h"
#include "names/gen2_items.h"
#endif

#ifdef PKSAVE_GEN3
#include "names/gen3_species.h"
#include "names/gen3_moves.h"
#include "names/gen3_items.h"
#include "names/gen3_abilities.h"
#endif

#ifdef PKSAVE_GEN4
#include "names/gen4_species.h"
#include "names/gen4_moves.h"
#include "names/gen4_items.h"
#include "names/gen4_abilities.h"
#endif

#ifdef PKSAVE_GEN5
#include "names/gen5_species.h"
#include "names/gen5_moves.h"
#include "names/gen5_items.h"
#include "names/gen5_abilities.h"
#endif

/* Natures are the same across Gen III-V */
static const char * const nature_names[25] = {
    "Hardy","Lonely","Brave","Adamant","Naughty",
    "Bold","Docile","Relaxed","Impish","Lax",
    "Timid","Hasty","Serious","Jolly","Naive",
    "Modest","Mild","Quiet","Bashful","Rash",
    "Calm","Gentle","Sassy","Careful","Quirky",
};

static const char *lookup(const char * const *table, size_t table_len,
                          uint16_t id)
{
    if (!table || id >= table_len) return "???";
    return table[id] ? table[id] : "???";
}

const char *pksave_species_name(pksave_game_t game, uint16_t id)
{
    switch (game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:
            return lookup(gen1_species_names,
                          sizeof(gen1_species_names)/sizeof(*gen1_species_names), id);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:
            return lookup(gen2_species_names,
                          sizeof(gen2_species_names)/sizeof(*gen2_species_names), id);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:
            return lookup(gen3_species_names,
                          sizeof(gen3_species_names)/sizeof(*gen3_species_names), id);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:
            return lookup(gen4_species_names,
                          sizeof(gen4_species_names)/sizeof(*gen4_species_names), id);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:
            return lookup(gen5_species_names,
                          sizeof(gen5_species_names)/sizeof(*gen5_species_names), id);
#endif
        default: return "???";
    }
}

const char *pksave_move_name(pksave_game_t game, uint16_t id)
{
    switch (game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:
            return lookup(gen1_move_names,
                          sizeof(gen1_move_names)/sizeof(*gen1_move_names), id);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:
            return lookup(gen2_move_names,
                          sizeof(gen2_move_names)/sizeof(*gen2_move_names), id);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:
            return lookup(gen3_move_names,
                          sizeof(gen3_move_names)/sizeof(*gen3_move_names), id);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:
            return lookup(gen4_move_names,
                          sizeof(gen4_move_names)/sizeof(*gen4_move_names), id);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:
            return lookup(gen5_move_names,
                          sizeof(gen5_move_names)/sizeof(*gen5_move_names), id);
#endif
        default: return "???";
    }
}

const char *pksave_item_name(pksave_game_t game, uint16_t id)
{
    switch (game) {
#ifdef PKSAVE_GEN1
        case PKSAVE_GAME_RB:
        case PKSAVE_GAME_YELLOW:
            return lookup(gen1_item_names,
                          sizeof(gen1_item_names)/sizeof(*gen1_item_names), id);
#endif
#ifdef PKSAVE_GEN2
        case PKSAVE_GAME_GS:
        case PKSAVE_GAME_CRYSTAL:
            return lookup(gen2_item_names,
                          sizeof(gen2_item_names)/sizeof(*gen2_item_names), id);
#endif
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:
            return lookup(gen3_item_names,
                          sizeof(gen3_item_names)/sizeof(*gen3_item_names), id);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:
            return lookup(gen4_item_names,
                          sizeof(gen4_item_names)/sizeof(*gen4_item_names), id);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:
            return lookup(gen5_item_names,
                          sizeof(gen5_item_names)/sizeof(*gen5_item_names), id);
#endif
        default: return "???";
    }
}

const char *pksave_ability_name(pksave_game_t game, uint8_t id)
{
    /* Gen I/II have no abilities; return "???" */
    switch (game) {
#ifdef PKSAVE_GEN3
        case PKSAVE_GAME_RS:
        case PKSAVE_GAME_EMERALD:
        case PKSAVE_GAME_FRLG:
            return lookup(gen3_ability_names,
                          sizeof(gen3_ability_names)/sizeof(*gen3_ability_names), id);
#endif
#ifdef PKSAVE_GEN4
        case PKSAVE_GAME_DP:
        case PKSAVE_GAME_PLATINUM:
        case PKSAVE_GAME_HGSS:
            return lookup(gen4_ability_names,
                          sizeof(gen4_ability_names)/sizeof(*gen4_ability_names), id);
#endif
#ifdef PKSAVE_GEN5
        case PKSAVE_GAME_BW:
        case PKSAVE_GAME_BW2:
            return lookup(gen5_ability_names,
                          sizeof(gen5_ability_names)/sizeof(*gen5_ability_names), id);
#endif
        default: return "???";
    }
}

const char *pksave_nature_name(uint8_t id)
{
    if (id >= 25) return "???";
    return nature_names[id];
}
