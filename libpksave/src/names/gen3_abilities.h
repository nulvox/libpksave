/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen 3 abilities name table indexed by the in-cartridge ability ID.
 * Sourced from pret/pokeemerald:
 *   include/constants/abilities.h (ID enum, ABILITY_NONE=0 .. ABILITY_AIR_LOCK=77)
 *   src/data/text/abilities.h     (gAbilityNames string array)
 *
 * pret stores the names ALL CAPS to match the Gen 3 in-game charset.
 * libpksave normalises to title case to match the convention already used
 * by gen3_species_names / nature_names — frontends do not re-case strings,
 * so what's in this table is what shows on screen.
 *
 * "Compoundeyes" and "Lightningrod" are single words in pret (cartridge
 * accurate); preserved here. Per the user-memory pret-canonical rule,
 * disagreements with other sources resolve in pret's favour.
 *
 * Index 0 (ABILITY_NONE) renders as "???" instead of pret's "-------"
 * because the editor's ability cycler filters slot-1 id-0 out (it means
 * "this species has no second ability"); a stray 0 reaching the renderer
 * is an unexpected state, and the "???" fallback matches the lookup()
 * helper in names.c for that case.
 */
#ifdef PKSAVE_GEN3
static const char * const gen3_ability_names[] = {
    /*   0 */ "???",
    /*   1 */ "Stench",
    /*   2 */ "Drizzle",
    /*   3 */ "Speed Boost",
    /*   4 */ "Battle Armor",
    /*   5 */ "Sturdy",
    /*   6 */ "Damp",
    /*   7 */ "Limber",
    /*   8 */ "Sand Veil",
    /*   9 */ "Static",
    /*  10 */ "Volt Absorb",
    /*  11 */ "Water Absorb",
    /*  12 */ "Oblivious",
    /*  13 */ "Cloud Nine",
    /*  14 */ "Compoundeyes",
    /*  15 */ "Insomnia",
    /*  16 */ "Color Change",
    /*  17 */ "Immunity",
    /*  18 */ "Flash Fire",
    /*  19 */ "Shield Dust",
    /*  20 */ "Own Tempo",
    /*  21 */ "Suction Cups",
    /*  22 */ "Intimidate",
    /*  23 */ "Shadow Tag",
    /*  24 */ "Rough Skin",
    /*  25 */ "Wonder Guard",
    /*  26 */ "Levitate",
    /*  27 */ "Effect Spore",
    /*  28 */ "Synchronize",
    /*  29 */ "Clear Body",
    /*  30 */ "Natural Cure",
    /*  31 */ "Lightningrod",
    /*  32 */ "Serene Grace",
    /*  33 */ "Swift Swim",
    /*  34 */ "Chlorophyll",
    /*  35 */ "Illuminate",
    /*  36 */ "Trace",
    /*  37 */ "Huge Power",
    /*  38 */ "Poison Point",
    /*  39 */ "Inner Focus",
    /*  40 */ "Magma Armor",
    /*  41 */ "Water Veil",
    /*  42 */ "Magnet Pull",
    /*  43 */ "Soundproof",
    /*  44 */ "Rain Dish",
    /*  45 */ "Sand Stream",
    /*  46 */ "Pressure",
    /*  47 */ "Thick Fat",
    /*  48 */ "Early Bird",
    /*  49 */ "Flame Body",
    /*  50 */ "Run Away",
    /*  51 */ "Keen Eye",
    /*  52 */ "Hyper Cutter",
    /*  53 */ "Pickup",
    /*  54 */ "Truant",
    /*  55 */ "Hustle",
    /*  56 */ "Cute Charm",
    /*  57 */ "Plus",
    /*  58 */ "Minus",
    /*  59 */ "Forecast",
    /*  60 */ "Sticky Hold",
    /*  61 */ "Shed Skin",
    /*  62 */ "Guts",
    /*  63 */ "Marvel Scale",
    /*  64 */ "Liquid Ooze",
    /*  65 */ "Overgrow",
    /*  66 */ "Blaze",
    /*  67 */ "Torrent",
    /*  68 */ "Swarm",
    /*  69 */ "Rock Head",
    /*  70 */ "Drought",
    /*  71 */ "Arena Trap",
    /*  72 */ "Vital Spirit",
    /*  73 */ "White Smoke",
    /*  74 */ "Pure Power",
    /*  75 */ "Shell Armor",
    /*  76 */ "Cacophony",
    /*  77 */ "Air Lock",
};
#endif
