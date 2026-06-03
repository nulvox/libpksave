/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen 2 tests — RED until TODO(3) implements gen2.c.
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

#define SRAM_SIZE (32*1024)
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

/* ── gs ── */
static uint8_t gs_buf[SRAM_SIZE];

void test_gs_trainer_round_trip(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
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

    s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_gs_party_slot0(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
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

void test_gs_checksum_valid_after_flush(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS));
}

/* ── crystal ── */
static uint8_t crystal_buf[SRAM_SIZE];

void test_crystal_trainer_round_trip(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
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

    s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_crystal_party_slot0(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
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

void test_crystal_checksum_valid_after_flush(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL));
}

void test_gs_party_slot_round_trip(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    party[0].species  = 152;  /* Chikorita (Gen II internal ID) */
    party[0].level    = 37;
    party[0].held_item = 200;
    party[0].move_ids[0] = 75;   /* Razor Leaf */
    party[0].move_pp[0]  = 25;
    party[0].evs[0]   = 54321;   /* stat exp passthrough 0-65535 */
    party[0].ivs[1]   = 30;      /* Atk DV=15 */
    party[0].ivs[5]   = 20;      /* Spe DV=10 */

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT16(152, party[0].species);
    TEST_ASSERT_EQUAL_UINT8(37, party[0].level);
    TEST_ASSERT_EQUAL_UINT16(200, party[0].held_item);
    TEST_ASSERT_EQUAL_UINT8(75, party[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(25, party[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT16(54321, party[0].evs[0]);
    TEST_ASSERT_EQUAL_UINT8(30, party[0].ivs[1]);
    TEST_ASSERT_EQUAL_UINT8(20, party[0].ivs[5]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS));
    pksave_close(s);
}

void test_crystal_party_slot_round_trip(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    party[0].species  = 158;  /* Totodile */
    party[0].level    = 50;
    party[0].move_ids[0] = 44;   /* Bite */
    party[0].move_pp[0]  = 20;

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT16(158, party[0].species);
    TEST_ASSERT_EQUAL_UINT8(50, party[0].level);
    TEST_ASSERT_EQUAL_UINT8(44, party[0].move_ids[0]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL));
    pksave_close(s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_gs_trainer_round_trip);
    RUN_TEST(test_gs_party_slot0);
    RUN_TEST(test_gs_checksum_valid_after_flush);
    RUN_TEST(test_gs_party_slot_round_trip);
    RUN_TEST(test_crystal_trainer_round_trip);
    RUN_TEST(test_crystal_party_slot0);
    RUN_TEST(test_crystal_checksum_valid_after_flush);
    RUN_TEST(test_crystal_party_slot_round_trip);
    return UNITY_END();
}
