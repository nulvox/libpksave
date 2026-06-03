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

/* Party-write round-trip: re-encrypt with checksum-seeded PRNG, write
 * the four 32-byte substructs.  Validates that an edited Pokémon decrypts
 * back to the values written and that the block CRC remains valid. */
static void gen4_party_round_trip(uint8_t *buf, pksave_game_t game,
                                   const char *fixture)
{
    if (!load_fixture(fixture, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    party[0].species  = 7;      /* Squirtle */
    party[0].level    = 60;
    party[0].held_item = 1;     /* Master Ball */
    party[0].move_ids[0] = 55;  /* Water Gun */
    party[0].move_pp[0]  = 25;
    party[0].evs[0]   = 200;
    party[0].evs[3]   = 150;
    party[0].ivs[0]   = 25;
    party[0].ivs[3]   = 31;
    party[0].stat[0]  = 200;

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT16(7, party[0].species);
    TEST_ASSERT_EQUAL_UINT8(60, party[0].level);
    TEST_ASSERT_EQUAL_UINT16(1, party[0].held_item);
    TEST_ASSERT_EQUAL_UINT8(55, party[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(25, party[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT8(200, party[0].evs[0]);
    TEST_ASSERT_EQUAL_UINT8(150, party[0].evs[3]);
    TEST_ASSERT_EQUAL_UINT8(25, party[0].ivs[0]);
    TEST_ASSERT_EQUAL_UINT8(31, party[0].ivs[3]);
    TEST_ASSERT_EQUAL_UINT16(200, party[0].stat[0]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(buf, SRAM_SIZE, game));
    pksave_close(s);
}

void test_dp_party_slot_round_trip(void)
    { gen4_party_round_trip(dp_buf, PKSAVE_GAME_DP, "dp.sav"); }
void test_platinum_party_slot_round_trip(void)
    { gen4_party_round_trip(platinum_buf, PKSAVE_GAME_PLATINUM, "platinum.sav"); }
void test_hgss_party_slot_round_trip(void)
    { gen4_party_round_trip(hgss_buf, PKSAVE_GAME_HGSS, "hgss.sav"); }

/* ── Gen IV PC box read ── */

void test_dp_box_count(void)
{
    if (!load_fixture("dp.sav", dp_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(18, pksave_get_box_count(s));
    pksave_close(s);
}

void test_dp_get_box_out_of_range(void)
{
    if (!load_fixture("dp.sav", dp_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_get_box(s, 18, box, &count));
    pksave_close(s);
}

static void gen4_box_with_mon(uint8_t *buf, pksave_game_t game, const char *fixture)
{
    if (!load_fixture(fixture, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(30, count);
    TEST_ASSERT_EQUAL_UINT16(1, box[0].species);    /* Bulbasaur */
    TEST_ASSERT_EQUAL_UINT8(33, box[0].move_ids[0]); /* Tackle */
    TEST_ASSERT_EQUAL_UINT16(0, box[1].species);    /* slot 1 empty */
    pksave_close(s);
}

void test_dp_get_box_with_mon(void)
    { gen4_box_with_mon(dp_buf, PKSAVE_GAME_DP, "dp.sav"); }
void test_platinum_get_box_with_mon(void)
    { gen4_box_with_mon(platinum_buf, PKSAVE_GAME_PLATINUM, "platinum.sav"); }
void test_hgss_get_box_with_mon(void)
    { gen4_box_with_mon(hgss_buf, PKSAVE_GAME_HGSS, "hgss.sav"); }

/* ── Gen IV PC box write ── */

static void gen4_box_round_trip(uint8_t *buf, pksave_game_t game, const char *fixture)
{
    if (!load_fixture(fixture, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));

    box[0].species     = 4;     /* Charmander */
    box[0].held_item   = 17;
    box[0].move_ids[0] = 10;    /* Scratch */
    box[0].move_pp[0]  = 35;
    box[0].evs[1]      = 100;
    box[0].ivs[1]      = 28;
    strcpy(box[0].nickname, "CHAR");

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_slot(s, 1, 0, &box[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT16(4, box[0].species);
    TEST_ASSERT_EQUAL_UINT16(17, box[0].held_item);
    TEST_ASSERT_EQUAL_UINT8(10, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(35, box[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT8(100, box[0].evs[1]);
    TEST_ASSERT_EQUAL_UINT8(28, box[0].ivs[1]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(buf, SRAM_SIZE, game));
    pksave_close(s);
}

void test_dp_set_box_slot_round_trip(void)
    { gen4_box_round_trip(dp_buf, PKSAVE_GAME_DP, "dp.sav"); }
void test_platinum_set_box_slot_round_trip(void)
    { gen4_box_round_trip(platinum_buf, PKSAVE_GAME_PLATINUM, "platinum.sav"); }
void test_hgss_set_box_slot_round_trip(void)
    { gen4_box_round_trip(hgss_buf, PKSAVE_GAME_HGSS, "hgss.sav"); }

void test_dp_set_box_slot_out_of_range(void)
{
    if (!load_fixture("dp.sav", dp_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t mon;
    memset(&mon, 0, sizeof(mon));
    mon.species = 1;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 18, 0, &mon));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 0, 30, &mon));
    pksave_close(s);
}

/* ── Gen 4 bag ── */

static void gen4_items_get(uint8_t *buf, pksave_game_t game, const char *name)
{
    if (!load_fixture(name, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[10];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(17,   items[0].item_id);  /* Potion (Gen 4 ID) */
    TEST_ASSERT_EQUAL_UINT16(5,    items[0].quantity);
    TEST_ASSERT_EQUAL_UINT16(0x4A, items[1].item_id);
    TEST_ASSERT_EQUAL_UINT16(2,    items[1].quantity);
    pksave_close(s);
}

void test_dp_get_items_pocket(void)
    { gen4_items_get(dp_buf, PKSAVE_GAME_DP, "dp.sav"); }
void test_platinum_get_items_pocket(void)
    { gen4_items_get(platinum_buf, PKSAVE_GAME_PLATINUM, "platinum.sav"); }
void test_hgss_get_items_pocket(void)
    { gen4_items_get(hgss_buf, PKSAVE_GAME_HGSS, "hgss.sav"); }

static void gen4_item_round_trip(uint8_t *buf, pksave_game_t game, const char *name)
{
    if (!load_fixture(name, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t new_item = { .item_id = 0x456, .quantity = 99 };
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_set_item(s, PKSAVE_POCKET_BALLS, 0, &new_item));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[16];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_BALLS, items, &count));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x456, items[0].item_id);
    TEST_ASSERT_EQUAL_UINT16(99,    items[0].quantity);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(buf, SRAM_SIZE, game));
    pksave_close(s);
}

void test_dp_set_item_round_trip(void)
    { gen4_item_round_trip(dp_buf, PKSAVE_GAME_DP, "dp.sav"); }
void test_platinum_set_item_round_trip(void)
    { gen4_item_round_trip(platinum_buf, PKSAVE_GAME_PLATINUM, "platinum.sav"); }
void test_hgss_set_item_round_trip(void)
    { gen4_item_round_trip(hgss_buf, PKSAVE_GAME_HGSS, "hgss.sav"); }

void test_dp_set_item_out_of_range(void)
{
    if (!load_fixture("dp.sav", dp_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(dp_buf, SRAM_SIZE, PKSAVE_GAME_DP);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t item = { .item_id = 1, .quantity = 1 };
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE,
        pksave_set_item(s, PKSAVE_POCKET_BALLS, 15, &item));
    pksave_close(s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dp_trainer_round_trip);
    RUN_TEST(test_dp_party_slot0);
    RUN_TEST(test_dp_checksum_valid_after_flush);
    RUN_TEST(test_dp_party_slot_round_trip);
    RUN_TEST(test_platinum_trainer_round_trip);
    RUN_TEST(test_platinum_party_slot0);
    RUN_TEST(test_platinum_checksum_valid_after_flush);
    RUN_TEST(test_platinum_party_slot_round_trip);
    RUN_TEST(test_hgss_trainer_round_trip);
    RUN_TEST(test_hgss_party_slot0);
    RUN_TEST(test_hgss_checksum_valid_after_flush);
    RUN_TEST(test_hgss_party_slot_round_trip);
    RUN_TEST(test_dp_box_count);
    RUN_TEST(test_dp_get_box_out_of_range);
    RUN_TEST(test_dp_get_box_with_mon);
    RUN_TEST(test_platinum_get_box_with_mon);
    RUN_TEST(test_hgss_get_box_with_mon);
    RUN_TEST(test_dp_set_box_slot_round_trip);
    RUN_TEST(test_platinum_set_box_slot_round_trip);
    RUN_TEST(test_hgss_set_box_slot_round_trip);
    RUN_TEST(test_dp_set_box_slot_out_of_range);
    RUN_TEST(test_dp_get_items_pocket);
    RUN_TEST(test_platinum_get_items_pocket);
    RUN_TEST(test_hgss_get_items_pocket);
    RUN_TEST(test_dp_set_item_round_trip);
    RUN_TEST(test_platinum_set_item_round_trip);
    RUN_TEST(test_hgss_set_item_round_trip);
    RUN_TEST(test_dp_set_item_out_of_range);
    return UNITY_END();
}
