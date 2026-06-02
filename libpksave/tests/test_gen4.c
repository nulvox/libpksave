/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen 4 tests — RED until TODO(3) implements gen4.c.
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

/* ── dp ── */
static uint8_t dp_buf[SRAM_SIZE];

void test_dp_trainer_round_trip(void)
{
    if (!load_fixture("dp.sav", dp_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP);
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

    s = pksave_open(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_dp_party_slot0(void)
{
    if (!load_fixture("dp.sav", dp_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP);
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

void test_dp_checksum_valid_after_flush(void)
{
    if (!load_fixture("dp.sav", dp_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP));
}

/* ── platinum ── */
static uint8_t platinum_buf[SRAM_SIZE];

void test_platinum_trainer_round_trip(void)
{
    if (!load_fixture("platinum.sav", platinum_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(platinum_buf, SRAM_SIZE, PKSAVE_GAME_PLATINUM);
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

    s = pksave_open(platinum_buf, SRAM_SIZE, PKSAVE_GAME_PLATINUM);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_platinum_party_slot0(void)
{
    if (!load_fixture("platinum.sav", platinum_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(platinum_buf, SRAM_SIZE, PKSAVE_GAME_PLATINUM);
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

void test_platinum_checksum_valid_after_flush(void)
{
    if (!load_fixture("platinum.sav", platinum_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(platinum_buf, SRAM_SIZE, PKSAVE_GAME_PLATINUM);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(platinum_buf, SRAM_SIZE, PKSAVE_GAME_PLATINUM));
}

/* ── hgss ── */
static uint8_t hgss_buf[SRAM_SIZE];

void test_hgss_trainer_round_trip(void)
{
    if (!load_fixture("hgss.sav", hgss_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(hgss_buf, SRAM_SIZE, PKSAVE_GAME_HGSS);
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

    s = pksave_open(hgss_buf, SRAM_SIZE, PKSAVE_GAME_HGSS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_hgss_party_slot0(void)
{
    if (!load_fixture("hgss.sav", hgss_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(hgss_buf, SRAM_SIZE, PKSAVE_GAME_HGSS);
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

void test_hgss_checksum_valid_after_flush(void)
{
    if (!load_fixture("hgss.sav", hgss_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(hgss_buf, SRAM_SIZE, PKSAVE_GAME_HGSS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(hgss_buf, SRAM_SIZE, PKSAVE_GAME_HGSS));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dp_trainer_round_trip);
    RUN_TEST(test_dp_party_slot0);
    RUN_TEST(test_dp_checksum_valid_after_flush);
    RUN_TEST(test_platinum_trainer_round_trip);
    RUN_TEST(test_platinum_party_slot0);
    RUN_TEST(test_platinum_checksum_valid_after_flush);
    RUN_TEST(test_hgss_trainer_round_trip);
    RUN_TEST(test_hgss_party_slot0);
    RUN_TEST(test_hgss_checksum_valid_after_flush);
    return UNITY_END();
}
