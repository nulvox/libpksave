/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen 5 tests — RED until TODO(3) implements gen5.c.
 *
 * Canonical test pattern per AGENT_PROMPT.md §Testing Requirements:
 *   1. open fixture → assert trainer name/money/badges
 *   2. read party slot 0 → assert species/level/move
 *   3. mutate money → flush → reload → assert persisted
 *   4. assert checksum valid after flush
 */
#include "unity.h"
#include "pksave.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define SRAM_SIZE (512*1024)
#define FIXTURE_DIR "tests/fixtures/"

static int load_fixture(const char *name, uint8_t *buf, size_t len)
{
    char path[256];
    snprintf(path, sizeof(path), FIXTURE_DIR "%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) { printf("SKIP: fixture not found: %s\n", path); return 0; }
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    if (n != len) {
        printf("SKIP: fixture wrong size: %s (got %zu, want %zu)\n",
               path, n, len);
        return 0;
    }
    return 1;
}

void setUp(void)    {}
void tearDown(void) {}

/* ── bw ── */
static uint8_t bw_buf[SRAM_SIZE];

void test_bw_trainer_round_trip(void)
{
    if (!load_fixture("bw.sav", bw_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw_buf, SRAM_SIZE, PKSAVE_GAME_BW);
    TEST_ASSERT_NOT_NULL(s);

    pksave_trainer_t tr;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    /* TODO(3): replace expected values with those from the fixture generator */
    TEST_ASSERT_GREATER_THAN(0, (int)tr.money);

    uint32_t new_money = 12345;
    tr.money = new_money;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_trainer(s, &tr));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(bw_buf, SRAM_SIZE, PKSAVE_GAME_BW);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_bw_party_slot0(void)
{
    if (!load_fixture("bw.sav", bw_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw_buf, SRAM_SIZE, PKSAVE_GAME_BW);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);
    /* TODO(3): assert specific species/level/move from fixture */
    TEST_ASSERT_GREATER_THAN(0, (int)party[0].species);
    TEST_ASSERT_GREATER_THAN(0, (int)party[0].level);
    pksave_close(s);
}

void test_bw_checksum_valid_after_flush(void)
{
    if (!load_fixture("bw.sav", bw_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw_buf, SRAM_SIZE, PKSAVE_GAME_BW);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(bw_buf, SRAM_SIZE, PKSAVE_GAME_BW));
}

/* ── bw2 ── */
static uint8_t bw2_buf[SRAM_SIZE];

void test_bw2_trainer_round_trip(void)
{
    if (!load_fixture("bw2.sav", bw2_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw2_buf, SRAM_SIZE, PKSAVE_GAME_BW2);
    TEST_ASSERT_NOT_NULL(s);

    pksave_trainer_t tr;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    /* TODO(3): replace expected values with those from the fixture generator */
    TEST_ASSERT_GREATER_THAN(0, (int)tr.money);

    uint32_t new_money = 12345;
    tr.money = new_money;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_trainer(s, &tr));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(bw2_buf, SRAM_SIZE, PKSAVE_GAME_BW2);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_bw2_party_slot0(void)
{
    if (!load_fixture("bw2.sav", bw2_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw2_buf, SRAM_SIZE, PKSAVE_GAME_BW2);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);
    /* TODO(3): assert specific species/level/move from fixture */
    TEST_ASSERT_GREATER_THAN(0, (int)party[0].species);
    TEST_ASSERT_GREATER_THAN(0, (int)party[0].level);
    pksave_close(s);
}

void test_bw2_checksum_valid_after_flush(void)
{
    if (!load_fixture("bw2.sav", bw2_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw2_buf, SRAM_SIZE, PKSAVE_GAME_BW2);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(bw2_buf, SRAM_SIZE, PKSAVE_GAME_BW2));
}

static void gen5_party_round_trip(uint8_t *buf, pksave_game_t game,
                                   const char *fixture)
{
    if (!load_fixture(fixture, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    party[0].species  = 495;    /* Snivy */
    party[0].level    = 75;
    party[0].held_item = 24;
    party[0].move_ids[0] = 33;  /* Tackle (move_ids is uint8_t in the public API) */
    party[0].move_pp[0]  = 20;
    party[0].evs[2]   = 252;
    party[0].evs[5]   = 252;
    party[0].ivs[2]   = 30;
    party[0].ivs[5]   = 31;
    party[0].stat[5]  = 180;

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT16(495, party[0].species);
    TEST_ASSERT_EQUAL_UINT8(75, party[0].level);
    TEST_ASSERT_EQUAL_UINT16(24, party[0].held_item);
    TEST_ASSERT_EQUAL_UINT8(33, party[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(20, party[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT8(252, party[0].evs[2]);
    TEST_ASSERT_EQUAL_UINT8(252, party[0].evs[5]);
    TEST_ASSERT_EQUAL_UINT8(30, party[0].ivs[2]);
    TEST_ASSERT_EQUAL_UINT8(31, party[0].ivs[5]);
    TEST_ASSERT_EQUAL_UINT16(180, party[0].stat[5]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(buf, SRAM_SIZE, game));
    pksave_close(s);
}

void test_bw_party_slot_round_trip(void)
    { gen5_party_round_trip(bw_buf, PKSAVE_GAME_BW, "bw.sav"); }
void test_bw2_party_slot_round_trip(void)
    { gen5_party_round_trip(bw2_buf, PKSAVE_GAME_BW2, "bw2.sav"); }

/* ── Gen V PC box read ── */

void test_bw_box_count(void)
{
    if (!load_fixture("bw.sav", bw_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw_buf, SRAM_SIZE, PKSAVE_GAME_BW);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(24, pksave_get_box_count(s));
    pksave_close(s);
}

void test_bw_get_box_out_of_range(void)
{
    if (!load_fixture("bw.sav", bw_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw_buf, SRAM_SIZE, PKSAVE_GAME_BW);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_get_box(s, 24, box, &count));
    pksave_close(s);
}

static void gen5_box_with_mon(uint8_t *buf, pksave_game_t game, const char *fixture)
{
    if (!load_fixture(fixture, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(30, count);
    TEST_ASSERT_EQUAL_UINT16(1, box[0].species);
    TEST_ASSERT_EQUAL_UINT8(33, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT16(0, box[1].species);
    pksave_close(s);
}

void test_bw_get_box_with_mon(void)
    { gen5_box_with_mon(bw_buf, PKSAVE_GAME_BW, "bw.sav"); }
void test_bw2_get_box_with_mon(void)
    { gen5_box_with_mon(bw2_buf, PKSAVE_GAME_BW2, "bw2.sav"); }

/* ── Gen V PC box write ── */

static void gen5_box_round_trip(uint8_t *buf, pksave_game_t game, const char *fixture)
{
    if (!load_fixture(fixture, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));

    box[0].species     = 7;     /* Squirtle */
    box[0].held_item   = 17;
    box[0].move_ids[0] = 33;    /* Tackle */
    box[0].move_pp[0]  = 35;
    box[0].evs[2]      = 75;
    box[0].ivs[2]      = 22;

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_slot(s, 1, 0, &box[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT16(7, box[0].species);
    TEST_ASSERT_EQUAL_UINT16(17, box[0].held_item);
    TEST_ASSERT_EQUAL_UINT8(33, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(35, box[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT8(75, box[0].evs[2]);
    TEST_ASSERT_EQUAL_UINT8(22, box[0].ivs[2]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(buf, SRAM_SIZE, game));
    pksave_close(s);
}

void test_bw_set_box_slot_round_trip(void)
    { gen5_box_round_trip(bw_buf, PKSAVE_GAME_BW, "bw.sav"); }
void test_bw2_set_box_slot_round_trip(void)
    { gen5_box_round_trip(bw2_buf, PKSAVE_GAME_BW2, "bw2.sav"); }

void test_bw_set_box_slot_out_of_range(void)
{
    if (!load_fixture("bw.sav", bw_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(bw_buf, SRAM_SIZE, PKSAVE_GAME_BW);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t mon;
    memset(&mon, 0, sizeof(mon));
    mon.species = 1;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 24, 0, &mon));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 0, 30, &mon));
    pksave_close(s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bw_trainer_round_trip);
    RUN_TEST(test_bw_party_slot0);
    RUN_TEST(test_bw_checksum_valid_after_flush);
    RUN_TEST(test_bw_party_slot_round_trip);
    RUN_TEST(test_bw2_trainer_round_trip);
    RUN_TEST(test_bw2_party_slot0);
    RUN_TEST(test_bw2_checksum_valid_after_flush);
    RUN_TEST(test_bw2_party_slot_round_trip);
    RUN_TEST(test_bw_box_count);
    RUN_TEST(test_bw_get_box_out_of_range);
    RUN_TEST(test_bw_get_box_with_mon);
    RUN_TEST(test_bw2_get_box_with_mon);
    RUN_TEST(test_bw_set_box_slot_round_trip);
    RUN_TEST(test_bw2_set_box_slot_round_trip);
    RUN_TEST(test_bw_set_box_slot_out_of_range);
    return UNITY_END();
}
