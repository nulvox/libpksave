/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Unit tests for pksave_recalc_stats — the Gen III stat formula:
 *   HP    = floor((2*Base + IV + EV/4) * Level / 100) + Level + 10
 *   Other = floor((2*Base + IV + EV/4) * Level / 100) + 5,
 *           then * 1.1 (raised) or * 0.9 (lowered) per nature.
 *
 * Tests run on bare pksave_pokemon_t structs (no fixture I/O); the
 * formula is pure function of (species, level, nature, IVs, EVs).
 *
 * Expected values cross-checked against pret/pokeemerald engine and
 * the standard competitive-Pokemon reference at Level 100 IV=31 EV=252.
 */
#include "unity.h"
#include "pksave.h"
#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/* Bulbasaur (species=1), base 45/49/49/65/65/45 (HP/Atk/Def/SpA/SpD/Spe). */

void test_recalc_bulbasaur_level5_zero_neutral_hp(void)
{
    pksave_pokemon_t pkm;
    memset(&pkm, 0, sizeof(pkm));
    pkm.species = 1;
    pkm.level   = 5;
    pkm.nature  = 0;          /* Hardy (neutral) */

    pksave_recalc_stats(&pkm, PKSAVE_GAME_RS);

    /* HP = floor((2*45 + 0 + 0) * 5 / 100) + 5 + 10
         = floor(450/100) + 15 = 4 + 15 = 19 */
    TEST_ASSERT_EQUAL_UINT16(19, pkm.stat[0]);
}

void test_recalc_bulbasaur_level100_max_neutral_hp(void)
{
    pksave_pokemon_t pkm;
    memset(&pkm, 0, sizeof(pkm));
    pkm.species = 1;
    pkm.level   = 100;
    pkm.nature  = 0;          /* Hardy (neutral) */
    pkm.ivs[0]  = 31;
    pkm.evs[0]  = 252;

    pksave_recalc_stats(&pkm, PKSAVE_GAME_RS);

    /* HP = floor((2*45 + 31 + 252/4) * 100 / 100) + 100 + 10
         = (90 + 31 + 63) + 110 = 184 + 110 = 294 */
    TEST_ASSERT_EQUAL_UINT16(294, pkm.stat[0]);
}

void test_recalc_bulbasaur_level100_max_neutral_non_hp(void)
{
    pksave_pokemon_t pkm;
    memset(&pkm, 0, sizeof(pkm));
    pkm.species = 1;
    pkm.level   = 100;
    pkm.nature  = 0;          /* Hardy (neutral) */
    for (int i = 1; i < 6; i++) { pkm.ivs[i] = 31; pkm.evs[i] = 252; }

    pksave_recalc_stats(&pkm, PKSAVE_GAME_RS);

    /* Each non-HP stat = (2*Base + 31 + 252/4) * 100/100 + 5 = 2*Base + 99
     * (neutral nature ⇒ no ±10% mod).  Bulbasaur 49/49/65/65/45 → */
    TEST_ASSERT_EQUAL_UINT16(197, pkm.stat[1]);   /* Atk */
    TEST_ASSERT_EQUAL_UINT16(197, pkm.stat[2]);   /* Def */
    TEST_ASSERT_EQUAL_UINT16(229, pkm.stat[3]);   /* SpA */
    TEST_ASSERT_EQUAL_UINT16(229, pkm.stat[4]);   /* SpD */
    TEST_ASSERT_EQUAL_UINT16(189, pkm.stat[5]);   /* Spe */
}

void test_recalc_bulbasaur_level100_max_adamant(void)
{
    /* Adamant nature (id=3): raises Atk, lowers SpA, others unchanged.
     * HP is never affected by nature. */
    pksave_pokemon_t pkm;
    memset(&pkm, 0, sizeof(pkm));
    pkm.species = 1;
    pkm.level   = 100;
    pkm.nature  = 3;
    for (int i = 0; i < 6; i++) { pkm.ivs[i] = 31; pkm.evs[i] = 252; }

    pksave_recalc_stats(&pkm, PKSAVE_GAME_RS);

    TEST_ASSERT_EQUAL_UINT16(294, pkm.stat[0]);   /* HP unchanged */
    TEST_ASSERT_EQUAL_UINT16(216, pkm.stat[1]);   /* Atk = floor(197*1.1) */
    TEST_ASSERT_EQUAL_UINT16(197, pkm.stat[2]);   /* Def unchanged */
    TEST_ASSERT_EQUAL_UINT16(206, pkm.stat[3]);   /* SpA = floor(229*0.9) */
    TEST_ASSERT_EQUAL_UINT16(229, pkm.stat[4]);   /* SpD unchanged */
    TEST_ASSERT_EQUAL_UINT16(189, pkm.stat[5]);   /* Spe unchanged */
}

void test_recalc_bulbasaur_level100_max_bold(void)
{
    /* Bold nature (id=5): raises Def, lowers Atk. */
    pksave_pokemon_t pkm;
    memset(&pkm, 0, sizeof(pkm));
    pkm.species = 1;
    pkm.level   = 100;
    pkm.nature  = 5;
    for (int i = 0; i < 6; i++) { pkm.ivs[i] = 31; pkm.evs[i] = 252; }

    pksave_recalc_stats(&pkm, PKSAVE_GAME_RS);

    TEST_ASSERT_EQUAL_UINT16(177, pkm.stat[1]);   /* Atk = floor(197*0.9) */
    TEST_ASSERT_EQUAL_UINT16(216, pkm.stat[2]);   /* Def = floor(197*1.1) */
    TEST_ASSERT_EQUAL_UINT16(229, pkm.stat[3]);   /* SpA unchanged */
    TEST_ASSERT_EQUAL_UINT16(229, pkm.stat[4]);   /* SpD unchanged */
    TEST_ASSERT_EQUAL_UINT16(189, pkm.stat[5]);   /* Spe unchanged */
}

void test_recalc_charmander_level50_neutral(void)
{
    /* Charmander (4) base: 39/52/43/60/50/65 (per pret Gen 3).
     * Level 50, IV=31, EV=252 across the board, Hardy.
     *   HP  = floor((78+31+63)*50/100) + 50 + 10 = 86 + 60 = 146
     *   Atk = floor((104+31+63)*50/100) + 5      = 99 + 5  = 104
     *   Spe = floor((130+31+63)*50/100) + 5      = 112 + 5 = 117 */
    pksave_pokemon_t pkm;
    memset(&pkm, 0, sizeof(pkm));
    pkm.species = 4;
    pkm.level   = 50;
    pkm.nature  = 0;
    for (int i = 0; i < 6; i++) { pkm.ivs[i] = 31; pkm.evs[i] = 252; }

    pksave_recalc_stats(&pkm, PKSAVE_GAME_RS);
    TEST_ASSERT_EQUAL_UINT16(146, pkm.stat[0]);
    TEST_ASSERT_EQUAL_UINT16(104, pkm.stat[1]);
    TEST_ASSERT_EQUAL_UINT16(117, pkm.stat[5]);
}

void test_recalc_treecko_level50_neutral_hp(void)
{
    /* Treecko (Hoenn species id=277) base HP=40.
     *   HP = floor((80+31+63)*50/100) + 50 + 10 = 87 + 60 = 147 */
    pksave_pokemon_t pkm;
    memset(&pkm, 0, sizeof(pkm));
    pkm.species = 277;
    pkm.level   = 50;
    pkm.nature  = 0;
    for (int i = 0; i < 6; i++) { pkm.ivs[i] = 31; pkm.evs[i] = 252; }

    pksave_recalc_stats(&pkm, PKSAVE_GAME_RS);
    TEST_ASSERT_EQUAL_UINT16(147, pkm.stat[0]);
}

void test_recalc_unknown_species_zeros_stats(void)
{
    /* Unknown species ID returns all-zero stats. */
    pksave_pokemon_t pkm;
    memset(&pkm, 0, sizeof(pkm));
    /* Fill stats with junk to confirm they're cleared. */
    for (int i = 0; i < 6; i++) pkm.stat[i] = 9999;
    pkm.species = 9999;
    pkm.level   = 50;

    pksave_recalc_stats(&pkm, PKSAVE_GAME_RS);
    for (int i = 0; i < 6; i++) TEST_ASSERT_EQUAL_UINT16(0, pkm.stat[i]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_recalc_bulbasaur_level5_zero_neutral_hp);
    RUN_TEST(test_recalc_bulbasaur_level100_max_neutral_hp);
    RUN_TEST(test_recalc_bulbasaur_level100_max_neutral_non_hp);
    RUN_TEST(test_recalc_bulbasaur_level100_max_adamant);
    RUN_TEST(test_recalc_bulbasaur_level100_max_bold);
    RUN_TEST(test_recalc_charmander_level50_neutral);
    RUN_TEST(test_recalc_treecko_level50_neutral_hp);
    RUN_TEST(test_recalc_unknown_species_zeros_stats);
    return UNITY_END();
}
