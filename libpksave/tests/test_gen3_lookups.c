/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Tests for Gen 3 per-species and per-move lookups
 * (pksave_species_abilities, pksave_move_base_pp).
 *
 * Source of truth: pret/pokeruby src/data/pokemon/base_stats.h
 *                  + pret/pokeruby include/constants/abilities.h
 *                  + pret/pokeemerald src/data/battle_moves.h.
 *
 * Numeric IDs (not names) are asserted directly so the tests stay
 * stable if the ability/move name tables are later regenerated.
 */
#include "unity.h"
#include "pksave.h"
#include <stdint.h>

/* ── Gen 3 ability IDs from pret/pokeruby include/constants/abilities.h ─ */
#define ABILITY_NONE         0
#define ABILITY_STATIC       9
#define ABILITY_SHIELD_DUST  19
#define ABILITY_RUN_AWAY     50
#define ABILITY_GUTS         62
#define ABILITY_OVERGROW     65

void setUp(void)    {}
void tearDown(void) {}

/* ── pksave_species_abilities ────────────────────────────────────────── */

void test_species_abilities_bulbasaur_single_ability(void)
{
    /* Bulbasaur (species 1): Overgrow only, slot1 is 0. */
    pksave_species_abilities_t a =
        pksave_species_abilities(PKSAVE_GAME_RS, 1);
    TEST_ASSERT_EQUAL_UINT8(ABILITY_OVERGROW, a.slot0);
    TEST_ASSERT_EQUAL_UINT8(ABILITY_NONE,     a.slot1);
}

void test_species_abilities_pikachu_static(void)
{
    /* Pikachu (species 25): Static only. */
    pksave_species_abilities_t a =
        pksave_species_abilities(PKSAVE_GAME_EMERALD, 25);
    TEST_ASSERT_EQUAL_UINT8(ABILITY_STATIC, a.slot0);
    TEST_ASSERT_EQUAL_UINT8(ABILITY_NONE,   a.slot1);
}

void test_species_abilities_rattata_two_abilities(void)
{
    /* Rattata (species 19): Run Away + Guts.  Confirms the slot1 column
     * actually populates with a distinct, non-zero value for species
     * that have two abilities. */
    pksave_species_abilities_t a =
        pksave_species_abilities(PKSAVE_GAME_FRLG, 19);
    TEST_ASSERT_EQUAL_UINT8(ABILITY_RUN_AWAY, a.slot0);
    TEST_ASSERT_EQUAL_UINT8(ABILITY_GUTS,     a.slot1);
}

void test_species_abilities_unknown_species_returns_zero(void)
{
    /* Out-of-range species ID returns {0,0}. */
    pksave_species_abilities_t a =
        pksave_species_abilities(PKSAVE_GAME_RS, 65535);
    TEST_ASSERT_EQUAL_UINT8(0, a.slot0);
    TEST_ASSERT_EQUAL_UINT8(0, a.slot1);
}

void test_species_abilities_placeholder_old_unown_returns_zero(void)
{
    /* OLD_UNOWN placeholders (species 252-276) and Egg (412) stay zeroed. */
    pksave_species_abilities_t a =
        pksave_species_abilities(PKSAVE_GAME_RS, 260);
    TEST_ASSERT_EQUAL_UINT8(0, a.slot0);
    TEST_ASSERT_EQUAL_UINT8(0, a.slot1);

    pksave_species_abilities_t egg =
        pksave_species_abilities(PKSAVE_GAME_RS, 412);
    TEST_ASSERT_EQUAL_UINT8(0, egg.slot0);
    TEST_ASSERT_EQUAL_UINT8(0, egg.slot1);
}

void test_species_abilities_non_gen3_game_returns_zero(void)
{
    /* Abilities don't exist in Gen I; lookup must return {0,0} regardless
     * of species ID. */
    pksave_species_abilities_t a =
        pksave_species_abilities(PKSAVE_GAME_RB, 1);
    TEST_ASSERT_EQUAL_UINT8(0, a.slot0);
    TEST_ASSERT_EQUAL_UINT8(0, a.slot1);
}

/* ── pksave_move_base_pp ─────────────────────────────────────────────── */

void test_move_base_pp_pound(void)
{
    TEST_ASSERT_EQUAL_UINT8(35, pksave_move_base_pp(PKSAVE_GAME_RS, 1));
}

void test_move_base_pp_thunderbolt(void)
{
    TEST_ASSERT_EQUAL_UINT8(15, pksave_move_base_pp(PKSAVE_GAME_EMERALD, 85));
}

void test_move_base_pp_tackle(void)
{
    TEST_ASSERT_EQUAL_UINT8(35, pksave_move_base_pp(PKSAVE_GAME_FRLG, 33));
}

void test_move_base_pp_struggle(void)
{
    /* Struggle has PP=1 per pret/pokeemerald — the game decrements it
     * after each use even though the move never runs out. */
    TEST_ASSERT_EQUAL_UINT8(1, pksave_move_base_pp(PKSAVE_GAME_RS, 165));
}

void test_move_base_pp_splash(void)
{
    TEST_ASSERT_EQUAL_UINT8(40, pksave_move_base_pp(PKSAVE_GAME_EMERALD, 150));
}

void test_move_base_pp_hidden_power(void)
{
    TEST_ASSERT_EQUAL_UINT8(15, pksave_move_base_pp(PKSAVE_GAME_RS, 237));
}

void test_move_base_pp_unknown_move_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, pksave_move_base_pp(PKSAVE_GAME_RS, 500));
    TEST_ASSERT_EQUAL_UINT8(0, pksave_move_base_pp(PKSAVE_GAME_RS, 65535));
}

void test_move_base_pp_move_zero_returns_zero(void)
{
    /* Move ID 0 is MOVE_NONE — no PP. */
    TEST_ASSERT_EQUAL_UINT8(0, pksave_move_base_pp(PKSAVE_GAME_RS, 0));
}

void test_move_base_pp_non_gen3_game_returns_zero(void)
{
    /* Non-Gen-3 games return 0 even for a valid Gen 3 move ID. */
    TEST_ASSERT_EQUAL_UINT8(0, pksave_move_base_pp(PKSAVE_GAME_RB, 1));
    TEST_ASSERT_EQUAL_UINT8(0, pksave_move_base_pp(PKSAVE_GAME_DP, 1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_species_abilities_bulbasaur_single_ability);
    RUN_TEST(test_species_abilities_pikachu_static);
    RUN_TEST(test_species_abilities_rattata_two_abilities);
    RUN_TEST(test_species_abilities_unknown_species_returns_zero);
    RUN_TEST(test_species_abilities_placeholder_old_unown_returns_zero);
    RUN_TEST(test_species_abilities_non_gen3_game_returns_zero);
    RUN_TEST(test_move_base_pp_pound);
    RUN_TEST(test_move_base_pp_thunderbolt);
    RUN_TEST(test_move_base_pp_tackle);
    RUN_TEST(test_move_base_pp_struggle);
    RUN_TEST(test_move_base_pp_splash);
    RUN_TEST(test_move_base_pp_hidden_power);
    RUN_TEST(test_move_base_pp_unknown_move_returns_zero);
    RUN_TEST(test_move_base_pp_move_zero_returns_zero);
    RUN_TEST(test_move_base_pp_non_gen3_game_returns_zero);
    return UNITY_END();
}
