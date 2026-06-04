/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Tests for per-game EV/IV caps and Gen III name-length macros.
 */
#include "unity.h"
#include "pksave.h"

void setUp(void)    {}
void tearDown(void) {}

void test_iv_max_gen3_rs_is_31(void)
{
    TEST_ASSERT_EQUAL_UINT8(31, pksave_iv_max(PKSAVE_GAME_RS));
}

void test_iv_max_gen1_rb_is_15(void)
{
    TEST_ASSERT_EQUAL_UINT8(15, pksave_iv_max(PKSAVE_GAME_RB));
}

void test_iv_max_unknown_is_0(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, pksave_iv_max(PKSAVE_GAME_UNKNOWN));
}

void test_ev_max_per_stat_gen3_rs_is_252(void)
{
    TEST_ASSERT_EQUAL_UINT16(252, pksave_ev_max_per_stat(PKSAVE_GAME_RS));
}

void test_ev_max_per_stat_gen1_rb_is_65535(void)
{
    TEST_ASSERT_EQUAL_UINT16(65535, pksave_ev_max_per_stat(PKSAVE_GAME_RB));
}

void test_ev_max_per_stat_unknown_is_0(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, pksave_ev_max_per_stat(PKSAVE_GAME_UNKNOWN));
}

void test_ev_total_max_gen3_rs_is_510(void)
{
    TEST_ASSERT_EQUAL_UINT16(510, pksave_ev_total_max(PKSAVE_GAME_RS));
}

void test_ev_total_max_gen1_rb_is_0_sentinel(void)
{
    /* Gen I/II stat exp has no total cap; sentinel value 0. */
    TEST_ASSERT_EQUAL_UINT16(0, pksave_ev_total_max(PKSAVE_GAME_RB));
}

void test_ev_total_max_unknown_is_0(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, pksave_ev_total_max(PKSAVE_GAME_UNKNOWN));
}

void test_gen3_name_length_macros(void)
{
    /* In-game character limits visible to the player (excluding terminator). */
    TEST_ASSERT_EQUAL_INT(7,  PKSAVE_GEN3_TRAINER_NAME_MAX);
    TEST_ASSERT_EQUAL_INT(10, PKSAVE_GEN3_NICKNAME_MAX);
    TEST_ASSERT_EQUAL_INT(7,  PKSAVE_GEN3_OT_NAME_MAX);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_iv_max_gen3_rs_is_31);
    RUN_TEST(test_iv_max_gen1_rb_is_15);
    RUN_TEST(test_iv_max_unknown_is_0);
    RUN_TEST(test_ev_max_per_stat_gen3_rs_is_252);
    RUN_TEST(test_ev_max_per_stat_gen1_rb_is_65535);
    RUN_TEST(test_ev_max_per_stat_unknown_is_0);
    RUN_TEST(test_ev_total_max_gen3_rs_is_510);
    RUN_TEST(test_ev_total_max_gen1_rb_is_0_sentinel);
    RUN_TEST(test_ev_total_max_unknown_is_0);
    RUN_TEST(test_gen3_name_length_macros);
    return UNITY_END();
}
