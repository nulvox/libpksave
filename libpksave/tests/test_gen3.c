/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen 3 tests — RED until TODO(3) implements gen3.c.
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

#define SRAM_SIZE (128*1024)
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

/* ── rs ── */
static uint8_t rs_buf[SRAM_SIZE];

void test_rs_trainer_round_trip(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
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

    s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_rs_party_slot0(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
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

void test_rs_checksum_valid_after_flush(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS));
}

/* ── emerald ── */
static uint8_t emerald_buf[SRAM_SIZE];

void test_emerald_trainer_round_trip(void)
{
    if (!load_fixture("emerald.sav", emerald_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
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

    s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_emerald_party_slot0(void)
{
    if (!load_fixture("emerald.sav", emerald_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
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

void test_emerald_checksum_valid_after_flush(void)
{
    if (!load_fixture("emerald.sav", emerald_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD));
}

/* ── frlg ── */
static uint8_t frlg_buf[SRAM_SIZE];

void test_frlg_trainer_round_trip(void)
{
    if (!load_fixture("frlg.sav", frlg_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
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

    s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_frlg_party_slot0(void)
{
    if (!load_fixture("frlg.sav", frlg_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
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

void test_frlg_checksum_valid_after_flush(void)
{
    if (!load_fixture("frlg.sav", frlg_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG));
}

/* Party-write round-trip: re-encrypt with PID^(TID|SID<<16) PRNG, recompute
 * section 1 checksum on flush.  Validates that an edited Pokémon decrypts
 * back to the values written, and that section checksums remain valid. */
static void gen3_party_round_trip(uint8_t *buf, pksave_game_t game,
                                   const char *fixture)
{
    if (!load_fixture(fixture, buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    party[0].species  = 4;      /* Charmander */
    party[0].level    = 50;
    party[0].held_item = 200;
    party[0].move_ids[0] = 52;  /* Ember */
    party[0].move_pp[0]  = 21;
    party[0].evs[0]   = 100;
    party[0].evs[1]   = 50;
    party[0].ivs[0]   = 31;
    party[0].ivs[1]   = 27;
    party[0].ivs[5]   = 11;
    party[0].stat[0]  = 123;    /* max HP */

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    /* Reopen and verify */
    s = pksave_open(buf, SRAM_SIZE, game);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT16(4, party[0].species);
    TEST_ASSERT_EQUAL_UINT8(50, party[0].level);
    TEST_ASSERT_EQUAL_UINT16(200, party[0].held_item);
    TEST_ASSERT_EQUAL_UINT8(52, party[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(21, party[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT8(100, party[0].evs[0]);
    TEST_ASSERT_EQUAL_UINT8(50, party[0].evs[1]);
    TEST_ASSERT_EQUAL_UINT8(31, party[0].ivs[0]);
    TEST_ASSERT_EQUAL_UINT8(27, party[0].ivs[1]);
    TEST_ASSERT_EQUAL_UINT8(11, party[0].ivs[5]);
    TEST_ASSERT_EQUAL_UINT16(123, party[0].stat[0]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(buf, SRAM_SIZE, game));
    pksave_close(s);
}

void test_rs_party_slot_round_trip(void)
    { gen3_party_round_trip(rs_buf, PKSAVE_GAME_RS, "rs.sav"); }
void test_emerald_party_slot_round_trip(void)
    { gen3_party_round_trip(emerald_buf, PKSAVE_GAME_EMERALD, "emerald.sav"); }
void test_frlg_party_slot_round_trip(void)
    { gen3_party_round_trip(frlg_buf, PKSAVE_GAME_FRLG, "frlg.sav"); }

/* ── Gen III PC box read ── */

void test_rs_box_count(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(14, pksave_get_box_count(s));
    pksave_close(s);
}

void test_rs_get_box_out_of_range(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_get_box(s, 14, box, &count));
    pksave_close(s);
}

void test_rs_get_box_with_mon(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    /* Fixture populates box 1 slot 0 with Bulbasaur (national dex 1) */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(30, count);             /* fixed 30-slot box */
    TEST_ASSERT_EQUAL_UINT16(1, box[0].species);    /* Bulbasaur */
    TEST_ASSERT_EQUAL_UINT8(33, box[0].move_ids[0]); /* Tackle */
    /* slot 1 should be empty (species 0) */
    TEST_ASSERT_EQUAL_UINT16(0, box[1].species);
    pksave_close(s);
}

void test_emerald_get_box_with_mon(void)
{
    if (!load_fixture("emerald.sav", emerald_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT16(1, box[0].species);
    pksave_close(s);
}

void test_frlg_get_box_with_mon(void)
{
    if (!load_fixture("frlg.sav", frlg_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT16(1, box[0].species);
    pksave_close(s);
}

/* ── Gen III PC box write ── */

void test_rs_set_box_slot_round_trip(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));

    /* Mutate slot 0 — keep PID/TID/SID from existing mon by reading then writing. */
    box[0].species      = 4;     /* Charmander */
    box[0].held_item    = 17;    /* Potion */
    box[0].move_ids[0]  = 10;    /* Scratch */
    box[0].move_pp[0]   = 35;
    box[0].evs[1]       = 12;
    box[0].ivs[1]       = 25;
    strcpy(box[0].nickname, "CHAR");

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_slot(s, 1, 0, &box[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT16(4, box[0].species);
    TEST_ASSERT_EQUAL_UINT16(17, box[0].held_item);
    TEST_ASSERT_EQUAL_UINT8(10, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(35, box[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT16(12, box[0].evs[1]);
    TEST_ASSERT_EQUAL_UINT8(25, box[0].ivs[1]);
    TEST_ASSERT_EQUAL_STRING("CHAR", box[0].nickname);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS));
    pksave_close(s);
}

void test_emerald_set_box_slot_round_trip(void)
{
    if (!load_fixture("emerald.sav", emerald_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));

    box[0].species     = 7;    /* Squirtle */
    box[0].move_ids[0] = 33;   /* Tackle */
    box[0].move_pp[0]  = 35;

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_slot(s, 1, 0, &box[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT16(7, box[0].species);
    TEST_ASSERT_EQUAL_UINT8(33, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD));
    pksave_close(s);
}

void test_frlg_set_box_slot_round_trip(void)
{
    if (!load_fixture("frlg.sav", frlg_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));

    box[0].species     = 25;   /* Pikachu */
    box[0].move_ids[0] = 84;   /* Thunder Shock */

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_slot(s, 1, 0, &box[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT16(25, box[0].species);
    TEST_ASSERT_EQUAL_UINT8(84, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG));
    pksave_close(s);
}

void test_rs_set_box_slot_out_of_range(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t mon;
    memset(&mon, 0, sizeof(mon));
    mon.species = 1;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 14, 0, &mon));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 0, 30, &mon));
    pksave_close(s);
}

/* ── Gen 3 bag (RS layout used for RS+Emerald fixtures, FRLG separate) ── */

void test_rs_get_items_pocket(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(13,   items[0].item_id); /* Potion (Gen 3 ID) */
    TEST_ASSERT_EQUAL_UINT16(5,    items[0].quantity);
    TEST_ASSERT_EQUAL_UINT16(0x44, items[1].item_id); /* Hyper Potion-ish */
    TEST_ASSERT_EQUAL_UINT16(2,    items[1].quantity);
    pksave_close(s);
}

void test_rs_get_balls_pocket(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[16];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_BALLS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(4, items[0].item_id);    /* Poke Ball */
    TEST_ASSERT_EQUAL_UINT16(10, items[0].quantity);
    pksave_close(s);
}

void test_emerald_get_items_pocket_with_xor(void)
{
    /* Emerald encrypts quantities with the section 0 security key.
     * The fixture writes pre-XORed values; the library must un-XOR.
     * Emerald shares the RS-layout fixture (2 items: Potion ×5, ×2). */
    if (!load_fixture("emerald.sav", emerald_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(13, items[0].item_id);  /* Potion */
    TEST_ASSERT_EQUAL_UINT16(5,  items[0].quantity); /* un-XORed quantity */
    pksave_close(s);
}

void test_frlg_get_items_pocket(void)
{
    if (!load_fixture("frlg.sav", frlg_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[42];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(13, items[0].item_id);
    TEST_ASSERT_EQUAL_UINT16(7,  items[0].quantity);
    pksave_close(s);
}

void test_rs_set_item_round_trip(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t new_item = { .item_id = 0xAB, .quantity = 17 };
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_set_item(s, PKSAVE_POCKET_ITEMS, 0, &new_item));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0xAB, items[0].item_id);
    TEST_ASSERT_EQUAL_UINT16(17,   items[0].quantity);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS));
    pksave_close(s);
}

void test_emerald_set_item_round_trip(void)
{
    if (!load_fixture("emerald.sav", emerald_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t new_item = { .item_id = 0xCD, .quantity = 99 };
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_set_item(s, PKSAVE_POCKET_BALLS, 0, &new_item));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[16];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_BALLS, items, &count));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0xCD, items[0].item_id);
    TEST_ASSERT_EQUAL_UINT16(99,   items[0].quantity);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD));
    pksave_close(s);
}

void test_rs_set_item_out_of_range(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t item = { .item_id = 1, .quantity = 1 };
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE,
        pksave_set_item(s, PKSAVE_POCKET_ITEMS, 30, &item));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE,
        pksave_set_item(s, PKSAVE_POCKET_BALLS, 16, &item));
    pksave_close(s);
}

/* ── Gen III name lookups ── */

void test_gen3_species_name_lookup(void)
{
    /* National dex order for #1-251; Hoenn species start at internal ID 277 */
    TEST_ASSERT_EQUAL_STRING("Bulbasaur",  pksave_species_name(PKSAVE_GAME_RS, 1));
    TEST_ASSERT_EQUAL_STRING("Pikachu",    pksave_species_name(PKSAVE_GAME_RS, 25));
    TEST_ASSERT_EQUAL_STRING("Mewtwo",     pksave_species_name(PKSAVE_GAME_EMERALD, 150));
    TEST_ASSERT_EQUAL_STRING("Celebi",     pksave_species_name(PKSAVE_GAME_RS, 251));
    TEST_ASSERT_EQUAL_STRING("Treecko",    pksave_species_name(PKSAVE_GAME_RS, 277));
    TEST_ASSERT_EQUAL_STRING("Blaziken",   pksave_species_name(PKSAVE_GAME_RS, 282));
    TEST_ASSERT_EQUAL_STRING("Swampert",   pksave_species_name(PKSAVE_GAME_EMERALD, 285));
    TEST_ASSERT_EQUAL_STRING("Rayquaza",   pksave_species_name(PKSAVE_GAME_EMERALD, 406));
    TEST_ASSERT_EQUAL_STRING("Deoxys",     pksave_species_name(PKSAVE_GAME_EMERALD, 410));
    TEST_ASSERT_EQUAL_STRING("Chimecho",   pksave_species_name(PKSAVE_GAME_EMERALD, 411));
    TEST_ASSERT_EQUAL_STRING("Egg",        pksave_species_name(PKSAVE_GAME_EMERALD, 412));
    TEST_ASSERT_EQUAL_STRING("???",        pksave_species_name(PKSAVE_GAME_RS, 65535));
}

void test_gen3_move_name_lookup(void)
{
    TEST_ASSERT_EQUAL_STRING("Pound",        pksave_move_name(PKSAVE_GAME_RS, 1));
    TEST_ASSERT_EQUAL_STRING("Thunderbolt",  pksave_move_name(PKSAVE_GAME_RS, 85));
    TEST_ASSERT_EQUAL_STRING("Struggle",     pksave_move_name(PKSAVE_GAME_EMERALD, 165));
    TEST_ASSERT_EQUAL_STRING("Hidden Power", pksave_move_name(PKSAVE_GAME_RS, 237));
    TEST_ASSERT_EQUAL_STRING("Volt Tackle",  pksave_move_name(PKSAVE_GAME_EMERALD, 344));
    TEST_ASSERT_EQUAL_STRING("Psycho Boost", pksave_move_name(PKSAVE_GAME_EMERALD, 354));
    TEST_ASSERT_EQUAL_STRING("???",          pksave_move_name(PKSAVE_GAME_RS, 0));
    TEST_ASSERT_EQUAL_STRING("???",          pksave_move_name(PKSAVE_GAME_RS, 500));
}

void test_gen3_item_name_lookup(void)
{
    TEST_ASSERT_EQUAL_STRING("Master Ball", pksave_item_name(PKSAVE_GAME_RS, 1));
    TEST_ASSERT_EQUAL_STRING("Potion",      pksave_item_name(PKSAVE_GAME_EMERALD, 13));
    TEST_ASSERT_EQUAL_STRING("Rare Candy",  pksave_item_name(PKSAVE_GAME_RS, 68));
    TEST_ASSERT_EQUAL_STRING("TM01",        pksave_item_name(PKSAVE_GAME_RS, 289));
    TEST_ASSERT_EQUAL_STRING("TM50",        pksave_item_name(PKSAVE_GAME_EMERALD, 338));
    TEST_ASSERT_EQUAL_STRING("HM01",        pksave_item_name(PKSAVE_GAME_RS, 339));
    TEST_ASSERT_EQUAL_STRING("HM08",        pksave_item_name(PKSAVE_GAME_EMERALD, 346));
    TEST_ASSERT_EQUAL_STRING("???",         pksave_item_name(PKSAVE_GAME_RS, 0));
}

/* ── Gen III PID-derived fields (nature / is_shiny / ability) ── */

/* Party slot 0 fixture: PID=0, TID=0x1234, SID=0x5678; iv_word with bit 31
 * clear (generator writes IV=10 for all stats — bit 31 is the ability slot).
 *   nature  = PID % 25                        = 0  (Hardy)
 *   shiny   = (pid^pid>>16^TID^SID) < 8       = false
 *   ability = (iv_word >> 31) & 1             = 0
 */
void test_rs_party_slot0_pid_derived_fields(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    TEST_ASSERT_EQUAL_UINT8(0, party[0].nature);
    TEST_ASSERT_EQUAL_UINT8(0, party[0].is_shiny);
    TEST_ASSERT_EQUAL_UINT8(0, party[0].ability);

    pksave_close(s);
}

/* Box 1 slot 0 fixture: PID=0x12345678, TID=0x1234, SID=0x5678.
 *   nature  = 0x12345678 % 25                       = 21 (Sassy)
 *   shiny   = (0x5678 ^ 0x1234 ^ 0x1234 ^ 0x5678)=0 < 8 → true
 *   ability = (iv_word >> 31) & 1                   = 0
 */
void test_rs_box_pid_derived_fields(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));

    TEST_ASSERT_EQUAL_UINT8(21, box[0].nature);
    TEST_ASSERT_EQUAL_UINT8(1,  box[0].is_shiny);
    TEST_ASSERT_EQUAL_UINT8(0,  box[0].ability);

    pksave_close(s);
}

/* ── Gen III pokerus ── */

void test_rs_party_pokerus_round_trip(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_pokerus(s, 0, 2, 4));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL_UINT8(2, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(4, pkrs.days);
    TEST_ASSERT_EQUAL(PKSAVE_PKRS_ACTIVE, pkrs.status);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS));
    pksave_close(s);
}

void test_emerald_party_pokerus_round_trip(void)
{
    if (!load_fixture("emerald.sav", emerald_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_pokerus(s, 0, 5, 0));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(emerald_buf, SRAM_SIZE, PKSAVE_GAME_EMERALD);
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL_UINT8(5, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(0, pkrs.days);
    TEST_ASSERT_EQUAL(PKSAVE_PKRS_CURED, pkrs.status);
    pksave_close(s);
}

void test_frlg_party_pokerus_round_trip(void)
{
    if (!load_fixture("frlg.sav", frlg_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_pokerus(s, 0, 1, 2));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(frlg_buf, SRAM_SIZE, PKSAVE_GAME_FRLG);
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL_UINT8(1, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(2, pkrs.days);
    pksave_close(s);
}

void test_rs_box_pokerus_round_trip(void)
{
    if (!load_fixture("rs.sav", rs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);

    /* Fixture places a Bulbasaur at box 1 slot 0 */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_pokerus(s, 1, 0, 7, 3));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS);
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box_pokerus(s, 1, 0, &pkrs));
    TEST_ASSERT_EQUAL_UINT8(7, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(3, pkrs.days);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rs_buf, SRAM_SIZE, PKSAVE_GAME_RS));
    pksave_close(s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rs_trainer_round_trip);
    RUN_TEST(test_rs_party_slot0);
    RUN_TEST(test_rs_checksum_valid_after_flush);
    RUN_TEST(test_rs_party_slot_round_trip);
    RUN_TEST(test_emerald_trainer_round_trip);
    RUN_TEST(test_emerald_party_slot0);
    RUN_TEST(test_emerald_checksum_valid_after_flush);
    RUN_TEST(test_emerald_party_slot_round_trip);
    RUN_TEST(test_frlg_trainer_round_trip);
    RUN_TEST(test_frlg_party_slot0);
    RUN_TEST(test_frlg_checksum_valid_after_flush);
    RUN_TEST(test_frlg_party_slot_round_trip);
    RUN_TEST(test_rs_box_count);
    RUN_TEST(test_rs_get_box_out_of_range);
    RUN_TEST(test_rs_get_box_with_mon);
    RUN_TEST(test_emerald_get_box_with_mon);
    RUN_TEST(test_frlg_get_box_with_mon);
    RUN_TEST(test_rs_set_box_slot_round_trip);
    RUN_TEST(test_emerald_set_box_slot_round_trip);
    RUN_TEST(test_frlg_set_box_slot_round_trip);
    RUN_TEST(test_rs_set_box_slot_out_of_range);
    RUN_TEST(test_rs_get_items_pocket);
    RUN_TEST(test_rs_get_balls_pocket);
    RUN_TEST(test_emerald_get_items_pocket_with_xor);
    RUN_TEST(test_frlg_get_items_pocket);
    RUN_TEST(test_rs_set_item_round_trip);
    RUN_TEST(test_emerald_set_item_round_trip);
    RUN_TEST(test_rs_set_item_out_of_range);
    RUN_TEST(test_gen3_species_name_lookup);
    RUN_TEST(test_gen3_move_name_lookup);
    RUN_TEST(test_gen3_item_name_lookup);
    RUN_TEST(test_rs_party_slot0_pid_derived_fields);
    RUN_TEST(test_rs_box_pid_derived_fields);
    RUN_TEST(test_rs_party_pokerus_round_trip);
    RUN_TEST(test_emerald_party_pokerus_round_trip);
    RUN_TEST(test_frlg_party_pokerus_round_trip);
    RUN_TEST(test_rs_box_pokerus_round_trip);
    return UNITY_END();
}
