/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Tests for Gen 3 per-species, per-move, and per-item lookups
 * (pksave_species_abilities, pksave_move_base_pp,
 *  pksave_item_max_stack, pksave_item_pocket).
 *
 * Source of truth: pret/pokeruby src/data/pokemon/base_stats.h
 *                  + pret/pokeruby include/constants/abilities.h
 *                  + pret/pokeemerald src/data/battle_moves.h
 *                  + pret/pokeemerald src/data/items.h.
 *
 * Numeric IDs (not names) are asserted directly so the tests stay
 * stable if the ability/move/item name tables are later regenerated.
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

/* ── Gen 3 item IDs from pret/pokeemerald include/constants/items.h ──── */
#define ITEM_NONE           0
#define ITEM_MASTER_BALL    1
#define ITEM_POTION         13
#define ITEM_CHERI_BERRY    133
#define ITEM_TM01           289
#define ITEM_HM01           339
#define ITEM_COIN_CASE      260
#define ITEM_BICYCLE        360

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

/* ── pksave_item_max_stack ───────────────────────────────────────────── */

void test_item_max_stack_master_ball(void)
{
    /* Balls have no .importance in pret, so cap is the default 99. */
    TEST_ASSERT_EQUAL_UINT16(99, pksave_item_max_stack(PKSAVE_GAME_RS,      ITEM_MASTER_BALL));
    TEST_ASSERT_EQUAL_UINT16(99, pksave_item_max_stack(PKSAVE_GAME_EMERALD, ITEM_MASTER_BALL));
    TEST_ASSERT_EQUAL_UINT16(99, pksave_item_max_stack(PKSAVE_GAME_FRLG,    ITEM_MASTER_BALL));
}

void test_item_max_stack_potion(void)
{
    /* Ordinary stackable item. */
    TEST_ASSERT_EQUAL_UINT16(99, pksave_item_max_stack(PKSAVE_GAME_EMERALD, ITEM_POTION));
}

void test_item_max_stack_hm01_locked_at_one(void)
{
    /* HMs have .importance = 1 in pret -> cap 1. */
    TEST_ASSERT_EQUAL_UINT16(1, pksave_item_max_stack(PKSAVE_GAME_RS,   ITEM_HM01));
    TEST_ASSERT_EQUAL_UINT16(1, pksave_item_max_stack(PKSAVE_GAME_FRLG, ITEM_HM01));
}

void test_item_max_stack_tm01_stacks_to_99(void)
{
    /* TMs have no .importance in pret -> default 99, even though they're
     * in POCKET_TM_HM alongside HMs. */
    TEST_ASSERT_EQUAL_UINT16(99, pksave_item_max_stack(PKSAVE_GAME_EMERALD, ITEM_TM01));
}

void test_item_max_stack_berry_caps_at_999(void)
{
    /* Cheri Berry -> .pocket = POCKET_BERRIES -> 999. */
    TEST_ASSERT_EQUAL_UINT16(999, pksave_item_max_stack(PKSAVE_GAME_RS,   ITEM_CHERI_BERRY));
    TEST_ASSERT_EQUAL_UINT16(999, pksave_item_max_stack(PKSAVE_GAME_FRLG, ITEM_CHERI_BERRY));
}

void test_item_max_stack_key_item_locked_at_one(void)
{
    /* Coin Case (RSE key item) and Bicycle (FRLG key item) both have
     * .importance = 1 -> cap 1.  Reads the pokeemerald table for both
     * games (RSE and FRLG agree on these). */
    TEST_ASSERT_EQUAL_UINT16(1, pksave_item_max_stack(PKSAVE_GAME_RS,   ITEM_COIN_CASE));
    TEST_ASSERT_EQUAL_UINT16(1, pksave_item_max_stack(PKSAVE_GAME_FRLG, ITEM_BICYCLE));
}

void test_item_max_stack_item_none_returns_zero(void)
{
    /* ID 0 is the "empty slot" sentinel for the editor — no real max. */
    TEST_ASSERT_EQUAL_UINT16(0, pksave_item_max_stack(PKSAVE_GAME_RS, ITEM_NONE));
}

void test_item_max_stack_unknown_id_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, pksave_item_max_stack(PKSAVE_GAME_RS, 500));
    TEST_ASSERT_EQUAL_UINT16(0, pksave_item_max_stack(PKSAVE_GAME_RS, 65535));
}

void test_item_max_stack_placeholder_id_returns_zero(void)
{
    /* IDs like 52 are "????????" placeholders in the libpksave name table
     * and have no pret entry — they fall through to the 0 sentinel. */
    TEST_ASSERT_EQUAL_UINT16(0, pksave_item_max_stack(PKSAVE_GAME_RS, 52));
}

void test_item_max_stack_non_gen3_game_returns_zero(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, pksave_item_max_stack(PKSAVE_GAME_RB, ITEM_POTION));
    TEST_ASSERT_EQUAL_UINT16(0, pksave_item_max_stack(PKSAVE_GAME_DP, ITEM_POTION));
}

/* ── pksave_item_pocket ──────────────────────────────────────────────── */

void test_item_pocket_master_ball_is_balls(void)
{
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_BALLS,
        pksave_item_pocket(PKSAVE_GAME_RS,      ITEM_MASTER_BALL));
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_BALLS,
        pksave_item_pocket(PKSAVE_GAME_EMERALD, ITEM_MASTER_BALL));
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_BALLS,
        pksave_item_pocket(PKSAVE_GAME_FRLG,    ITEM_MASTER_BALL));
}

void test_item_pocket_potion_is_items(void)
{
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_ITEMS,
        pksave_item_pocket(PKSAVE_GAME_EMERALD, ITEM_POTION));
}

void test_item_pocket_tm01_is_tmhm(void)
{
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_TM_HM,
        pksave_item_pocket(PKSAVE_GAME_RS, ITEM_TM01));
}

void test_item_pocket_hm01_is_tmhm(void)
{
    /* HMs share the TM/HM pocket with TMs — distinct only by .importance,
     * which max_stack is responsible for surfacing. */
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_TM_HM,
        pksave_item_pocket(PKSAVE_GAME_FRLG, ITEM_HM01));
}

void test_item_pocket_berry_is_berries(void)
{
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_BERRIES,
        pksave_item_pocket(PKSAVE_GAME_RS, ITEM_CHERI_BERRY));
}

void test_item_pocket_key_item_is_key(void)
{
    /* Both an RSE-only key item (Coin Case) and a FRLG-only key item
     * (Bicycle) report POCKET_KEY when queried under any Gen 3 variant —
     * RSE and FRLG agree on the pocket assignment for every shared ID. */
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_KEY,
        pksave_item_pocket(PKSAVE_GAME_RS,   ITEM_COIN_CASE));
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_KEY,
        pksave_item_pocket(PKSAVE_GAME_FRLG, ITEM_BICYCLE));
    /* Cross-check: Bicycle queried under RSE still returns POCKET_KEY (the
     * table is per-ID, not per-game; pocket assignment doesn't shift). */
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_KEY,
        pksave_item_pocket(PKSAVE_GAME_RS, ITEM_BICYCLE));
}

void test_item_pocket_item_none_is_none(void)
{
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_NONE,
        pksave_item_pocket(PKSAVE_GAME_RS, ITEM_NONE));
}

void test_item_pocket_unknown_id_is_none(void)
{
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_NONE,
        pksave_item_pocket(PKSAVE_GAME_RS, 500));
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_NONE,
        pksave_item_pocket(PKSAVE_GAME_RS, 65535));
}

void test_item_pocket_non_gen3_game_is_none(void)
{
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_NONE,
        pksave_item_pocket(PKSAVE_GAME_RB, ITEM_POTION));
    TEST_ASSERT_EQUAL_INT(PKSAVE_POCKET_NONE,
        pksave_item_pocket(PKSAVE_GAME_DP, ITEM_POTION));
}

/* ── pksave_ability_name ─────────────────────────────────────────────────
 *
 * Spot-checks across the Gen 3 ability table sourced from pret/pokeemerald
 * src/data/text/abilities.h. We don't enumerate all 78 entries — a few
 * load-bearing samples catch the failure modes that matter:
 *   - first / last valid IDs (1 = Stench, 77 = Air Lock) bracket the table
 *   - the two pret quirks "Compoundeyes" and "Lightningrod" (one word, not
 *     two) are explicitly asserted so a well-meaning future re-format that
 *     "fixes" them would fail loudly
 *   - the index-0 sentinel and out-of-range IDs both fall through to "???"
 *   - non-Gen-3 game IDs return "???" so a Gen 1/2 caller never sees a
 *     Gen 3 ability label leak across.
 */
void test_ability_name_first_and_last(void)
{
    TEST_ASSERT_EQUAL_STRING("Stench",   pksave_ability_name(PKSAVE_GAME_RS,      1));
    TEST_ASSERT_EQUAL_STRING("Air Lock", pksave_ability_name(PKSAVE_GAME_EMERALD, 77));
}

void test_ability_name_pret_one_word_spellings(void)
{
    /* The two cartridge-canonical single-word spellings. If a future
     * contributor "corrects" these to "Compound Eyes" / "Lightning Rod",
     * the user-memory pret-canonical rule says they're wrong. */
    TEST_ASSERT_EQUAL_STRING("Compoundeyes", pksave_ability_name(PKSAVE_GAME_RS, 14));
    TEST_ASSERT_EQUAL_STRING("Lightningrod", pksave_ability_name(PKSAVE_GAME_RS, 31));
}

void test_ability_name_starter_abilities(void)
{
    /* Overgrow / Blaze / Torrent — the three starter abilities. Every Gen
     * 3 player encounters these within the first 30 minutes; if any broke
     * the editor would be visibly wrong on the first Pokemon edit. */
    TEST_ASSERT_EQUAL_STRING("Overgrow", pksave_ability_name(PKSAVE_GAME_RS,   65));
    TEST_ASSERT_EQUAL_STRING("Blaze",    pksave_ability_name(PKSAVE_GAME_RS,   66));
    TEST_ASSERT_EQUAL_STRING("Torrent",  pksave_ability_name(PKSAVE_GAME_FRLG, 67));
}

void test_ability_name_id_zero_is_unknown_sentinel(void)
{
    /* The editor's ability cycler treats slot-1 id 0 as "this species has
     * no second ability" and filters it out, so the renderer should never
     * see id 0 in practice — but if it does, "???" is the agreed fallback
     * (see header comment in names/gen3_abilities.h). */
    TEST_ASSERT_EQUAL_STRING("???", pksave_ability_name(PKSAVE_GAME_RS, 0));
}

void test_ability_name_out_of_range_is_unknown(void)
{
    /* IDs 78+ are unassigned in Gen 3 (Cacophony=76, Air Lock=77 are the
     * tail). The lookup() guard returns "???" for any id past table_len. */
    TEST_ASSERT_EQUAL_STRING("???", pksave_ability_name(PKSAVE_GAME_RS,  78));
    TEST_ASSERT_EQUAL_STRING("???", pksave_ability_name(PKSAVE_GAME_RS, 255));
}

void test_ability_name_non_gen3_game_is_unknown(void)
{
    /* Gen 1 / Gen 2 have no abilities at all. The dispatch in names.c
     * returns "???" without indexing into the Gen 3 table. */
    TEST_ASSERT_EQUAL_STRING("???", pksave_ability_name(PKSAVE_GAME_RB, 1));
    TEST_ASSERT_EQUAL_STRING("???", pksave_ability_name(PKSAVE_GAME_GS, 1));
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
    RUN_TEST(test_item_max_stack_master_ball);
    RUN_TEST(test_item_max_stack_potion);
    RUN_TEST(test_item_max_stack_hm01_locked_at_one);
    RUN_TEST(test_item_max_stack_tm01_stacks_to_99);
    RUN_TEST(test_item_max_stack_berry_caps_at_999);
    RUN_TEST(test_item_max_stack_key_item_locked_at_one);
    RUN_TEST(test_item_max_stack_item_none_returns_zero);
    RUN_TEST(test_item_max_stack_unknown_id_returns_zero);
    RUN_TEST(test_item_max_stack_placeholder_id_returns_zero);
    RUN_TEST(test_item_max_stack_non_gen3_game_returns_zero);
    RUN_TEST(test_item_pocket_master_ball_is_balls);
    RUN_TEST(test_item_pocket_potion_is_items);
    RUN_TEST(test_item_pocket_tm01_is_tmhm);
    RUN_TEST(test_item_pocket_hm01_is_tmhm);
    RUN_TEST(test_item_pocket_berry_is_berries);
    RUN_TEST(test_item_pocket_key_item_is_key);
    RUN_TEST(test_item_pocket_item_none_is_none);
    RUN_TEST(test_item_pocket_unknown_id_is_none);
    RUN_TEST(test_item_pocket_non_gen3_game_is_none);
    RUN_TEST(test_ability_name_first_and_last);
    RUN_TEST(test_ability_name_pret_one_word_spellings);
    RUN_TEST(test_ability_name_starter_abilities);
    RUN_TEST(test_ability_name_id_zero_is_unknown_sentinel);
    RUN_TEST(test_ability_name_out_of_range_is_unknown);
    RUN_TEST(test_ability_name_non_gen3_game_is_unknown);
    return UNITY_END();
}
