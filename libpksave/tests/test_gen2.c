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

/* ── Gen II pokerus ── */

void test_gs_party_pokerus_round_trip(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_pokerus(s, 0, 2, 3));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL_UINT8(2, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(3, pkrs.days);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS));
    pksave_close(s);
}

void test_gs_pokerus_status_never_active_cured(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);

    /* Fixture mon starts uninfected → status = NEVER */
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_PKRS_NEVER, pkrs.status);
    TEST_ASSERT_EQUAL_UINT8(0, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(0, pkrs.days);

    /* strain > 0, days > 0 → ACTIVE */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_pokerus(s, 0, 1, 4));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_PKRS_ACTIVE, pkrs.status);

    /* strain > 0, days == 0 → CURED */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_pokerus(s, 0, 1, 0));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_PKRS_CURED, pkrs.status);
    pksave_close(s);
}

/* ── Gen II PC box read ── */

void test_gs_box_count(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(14, pksave_get_box_count(s));
    pksave_close(s);
}

void test_gs_get_box_out_of_range(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_get_box(s, 14, box, &count));
    pksave_close(s);
}

void test_gs_get_empty_box(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 99;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 0, box, &count));
    TEST_ASSERT_EQUAL_UINT8(0, count);
    pksave_close(s);
}

void test_gs_get_stored_box_with_mon(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    /* Fixture populates box 1 with one Bulbasaur */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x99, box[0].species);
    TEST_ASSERT_EQUAL_UINT8(33, box[0].move_ids[0]);
    pksave_close(s);
}

void test_crystal_get_stored_box_with_mon(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 7, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x9A, box[0].species);
    pksave_close(s);
}

void test_crystal_party_pokerus_round_trip(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_pokerus(s, 0, 4, 2));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL_UINT8(4, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(2, pkrs.days);
    TEST_ASSERT_EQUAL(PKSAVE_PKRS_ACTIVE, pkrs.status);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL));
    pksave_close(s);
}

void test_gs_box_pokerus_round_trip(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);

    /* Fixture populates box 1 slot 0 with a Bulbasaur */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_pokerus(s, 1, 0, 3, 0));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box_pokerus(s, 1, 0, &pkrs));
    TEST_ASSERT_EQUAL_UINT8(3, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(0, pkrs.days);
    TEST_ASSERT_EQUAL(PKSAVE_PKRS_CURED, pkrs.status);
    pksave_close(s);
}

void test_crystal_box_pokerus_round_trip(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_pokerus(s, 7, 0, 1, 1));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box_pokerus(s, 7, 0, &pkrs));
    TEST_ASSERT_EQUAL_UINT8(1, pkrs.strain);
    TEST_ASSERT_EQUAL_UINT8(1, pkrs.days);
    pksave_close(s);
}

/* ── Gen II PC box write ── */

void test_gs_set_box_slot_round_trip(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);

    box[0].species      = 158;   /* Totodile (Gen II ID) */
    box[0].held_item    = 200;
    box[0].move_ids[0]  = 44;    /* Bite */
    box[0].move_pp[0]   = 25;
    box[0].ivs[1]       = 30;    /* Atk DV=15 */
    strcpy(box[0].nickname, "BOXY");
    strcpy(box[0].ot_name, "ME");

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_slot(s, 1, 0, &box[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(158, box[0].species);
    TEST_ASSERT_EQUAL_UINT16(200, box[0].held_item);
    TEST_ASSERT_EQUAL_UINT8(44, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(25, box[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT8(30, box[0].ivs[1]);
    TEST_ASSERT_EQUAL_STRING("BOXY", box[0].nickname);
    TEST_ASSERT_EQUAL_STRING("ME", box[0].ot_name);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS));
    pksave_close(s);
}

void test_crystal_set_box_slot_round_trip(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 7, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);

    box[0].species     = 155;   /* Cyndaquil */
    box[0].move_ids[0] = 52;    /* Ember */
    box[0].move_pp[0]  = 25;

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_slot(s, 7, 0, &box[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 7, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(155, box[0].species);
    TEST_ASSERT_EQUAL_UINT8(52, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL));
    pksave_close(s);
}

void test_gs_set_box_slot_out_of_range(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t mon;
    memset(&mon, 0, sizeof(mon));
    mon.species = 1;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 14, 0, &mon));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 0, 30, &mon));
    pksave_close(s);
}

/* ── Gen 2 bag (GS items pocket) ── */

void test_gs_get_items_pocket(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);

    pksave_item_t items[20];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(0x12, items[0].item_id); /* Super Potion */
    TEST_ASSERT_EQUAL_UINT16(5,    items[0].quantity);
    TEST_ASSERT_EQUAL_UINT16(0x49, items[1].item_id); /* Repel (Gen 2 ID) */
    TEST_ASSERT_EQUAL_UINT16(2,    items[1].quantity);
    pksave_close(s);
}

void test_gs_get_balls_pocket(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[12];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_BALLS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x04, items[0].item_id); /* Poke Ball Gen2 */
    TEST_ASSERT_EQUAL_UINT16(7,    items[0].quantity);
    pksave_close(s);
}

void test_gs_get_key_pocket(void)
{
    /* Key items in Gen 2 have no quantity per entry — the library should
     * surface them with quantity=1 so frontends can iterate uniformly. */
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[25];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_KEY, items, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x07, items[0].item_id); /* Bicycle */
    TEST_ASSERT_EQUAL_UINT16(1,    items[0].quantity);
    pksave_close(s);
}

void test_gs_set_item_round_trip(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t new_item = { .item_id = 0x99, .quantity = 42 };
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_set_item(s, PKSAVE_POCKET_ITEMS, 0, &new_item));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[20];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x99, items[0].item_id);
    TEST_ASSERT_EQUAL_UINT16(42,   items[0].quantity);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS));
    pksave_close(s);
}

void test_gs_set_item_out_of_range(void)
{
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t item = { .item_id = 1, .quantity = 1 };
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE,
        pksave_set_item(s, PKSAVE_POCKET_ITEMS, 20, &item));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE,
        pksave_set_item(s, PKSAVE_POCKET_BALLS, 12, &item));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE,
        pksave_set_item(s, PKSAVE_POCKET_KEY,   25, &item));
    pksave_close(s);
}

void test_crystal_get_items_pocket(void)
{
    static uint8_t cry_buf2[SRAM_SIZE];
    if (!load_fixture("crystal.sav", cry_buf2, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(cry_buf2, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);

    pksave_item_t items[20];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x12, items[0].item_id);
    TEST_ASSERT_EQUAL_UINT16(3,    items[0].quantity);
    pksave_close(s);
}

void test_crystal_set_item_round_trip(void)
{
    static uint8_t cry_buf3[SRAM_SIZE];
    if (!load_fixture("crystal.sav", cry_buf3, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(cry_buf3, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t new_item = { .item_id = 0x55, .quantity = 9 };
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_set_item(s, PKSAVE_POCKET_BALLS, 0, &new_item));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(cry_buf3, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[12];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_BALLS, items, &count));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x55, items[0].item_id);
    TEST_ASSERT_EQUAL_UINT16(9,    items[0].quantity);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(cry_buf3, SRAM_SIZE, PKSAVE_GAME_CRYSTAL));
    pksave_close(s);
}

/* ── Gen II derived fields: is_shiny + stat exp passthrough ── */

void test_gen2_is_shiny_derived(void)
{
    /* Gen II shiny formula: Def DV = 10, Spd DV = 10, Spc DV = 10,
     * Atk DV ∈ {2,3,6,7,10,11,14,15}. DVs are stored as IV*2. */
    if (!load_fixture("gs.sav", gs_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    /* Shiny DV set: Atk=15, Def=10, Spd=10, Spc=10 */
    party[0].ivs[1] = 30;
    party[0].ivs[2] = 20;
    party[0].ivs[5] = 20;
    party[0].ivs[3] = party[0].ivs[4] = 20;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT8(1, party[0].is_shiny);

    /* Non-shiny: Atk DV 12 (not in shiny set) */
    party[0].ivs[1] = 24;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(gs_buf, SRAM_SIZE, PKSAVE_GAME_GS);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT8(0, party[0].is_shiny);
    pksave_close(s);
}

void test_gen2_stat_exp_passthrough(void)
{
    /* Gen II stat exp is 0-65535 per stat — must not be clamped. */
    if (!load_fixture("crystal.sav", crystal_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    party[0].evs[0] = 65535;
    party[0].evs[1] = 50000;
    party[0].evs[5] = 12345;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(crystal_buf, SRAM_SIZE, PKSAVE_GAME_CRYSTAL);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT16(65535, party[0].evs[0]);
    TEST_ASSERT_EQUAL_UINT16(50000, party[0].evs[1]);
    TEST_ASSERT_EQUAL_UINT16(12345, party[0].evs[5]);
    pksave_close(s);
}

/* ── Gen II name lookups ── */

void test_gen2_species_name_lookup(void)
{
    /* Gen 2 species IDs are national dex order */
    TEST_ASSERT_EQUAL_STRING("Bulbasaur",  pksave_species_name(PKSAVE_GAME_GS, 1));
    TEST_ASSERT_EQUAL_STRING("Pikachu",    pksave_species_name(PKSAVE_GAME_GS, 25));
    TEST_ASSERT_EQUAL_STRING("Mewtwo",     pksave_species_name(PKSAVE_GAME_CRYSTAL, 150));
    TEST_ASSERT_EQUAL_STRING("Chikorita",  pksave_species_name(PKSAVE_GAME_CRYSTAL, 152));
    TEST_ASSERT_EQUAL_STRING("Typhlosion", pksave_species_name(PKSAVE_GAME_GS, 157));
    TEST_ASSERT_EQUAL_STRING("Celebi",     pksave_species_name(PKSAVE_GAME_CRYSTAL, 251));
    TEST_ASSERT_EQUAL_STRING("Egg",        pksave_species_name(PKSAVE_GAME_GS, 253));
    TEST_ASSERT_EQUAL_STRING("???",        pksave_species_name(PKSAVE_GAME_GS, 0));
    TEST_ASSERT_EQUAL_STRING("???",        pksave_species_name(PKSAVE_GAME_GS, 254));
}

void test_gen2_move_name_lookup(void)
{
    TEST_ASSERT_EQUAL_STRING("Pound",        pksave_move_name(PKSAVE_GAME_GS, 1));
    TEST_ASSERT_EQUAL_STRING("Thunderbolt",  pksave_move_name(PKSAVE_GAME_GS, 85));
    TEST_ASSERT_EQUAL_STRING("Struggle",     pksave_move_name(PKSAVE_GAME_GS, 165));
    TEST_ASSERT_EQUAL_STRING("Sketch",       pksave_move_name(PKSAVE_GAME_GS, 166));
    TEST_ASSERT_EQUAL_STRING("Hidden Power", pksave_move_name(PKSAVE_GAME_GS, 237));
    TEST_ASSERT_EQUAL_STRING("Beat Up",      pksave_move_name(PKSAVE_GAME_GS, 251));
    TEST_ASSERT_EQUAL_STRING("???",          pksave_move_name(PKSAVE_GAME_GS, 0));
    TEST_ASSERT_EQUAL_STRING("???",          pksave_move_name(PKSAVE_GAME_GS, 252));
}

void test_gen2_item_name_lookup(void)
{
    TEST_ASSERT_EQUAL_STRING("Master Ball", pksave_item_name(PKSAVE_GAME_GS, 0x01));
    TEST_ASSERT_EQUAL_STRING("Poke Ball",   pksave_item_name(PKSAVE_GAME_GS, 0x05));
    TEST_ASSERT_EQUAL_STRING("Antidote",    pksave_item_name(PKSAVE_GAME_GS, 0x09));
    TEST_ASSERT_EQUAL_STRING("Potion",      pksave_item_name(PKSAVE_GAME_GS, 0x12));
    TEST_ASSERT_EQUAL_STRING("Rare Candy",  pksave_item_name(PKSAVE_GAME_GS, 0x20));
    TEST_ASSERT_EQUAL_STRING("GS Ball",     pksave_item_name(PKSAVE_GAME_CRYSTAL, 0x73));
    TEST_ASSERT_EQUAL_STRING("TM01",        pksave_item_name(PKSAVE_GAME_GS, 0xBF));
    TEST_ASSERT_EQUAL_STRING("HM01",        pksave_item_name(PKSAVE_GAME_GS, 0xF3));
    TEST_ASSERT_EQUAL_STRING("HM07",        pksave_item_name(PKSAVE_GAME_GS, 0xF9));
    TEST_ASSERT_EQUAL_STRING("???",         pksave_item_name(PKSAVE_GAME_GS, 0));
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
    RUN_TEST(test_gs_party_pokerus_round_trip);
    RUN_TEST(test_gs_pokerus_status_never_active_cured);
    RUN_TEST(test_crystal_party_pokerus_round_trip);
    RUN_TEST(test_gs_box_pokerus_round_trip);
    RUN_TEST(test_crystal_box_pokerus_round_trip);
    RUN_TEST(test_gs_box_count);
    RUN_TEST(test_gs_get_box_out_of_range);
    RUN_TEST(test_gs_get_empty_box);
    RUN_TEST(test_gs_get_stored_box_with_mon);
    RUN_TEST(test_crystal_get_stored_box_with_mon);
    RUN_TEST(test_gs_set_box_slot_round_trip);
    RUN_TEST(test_crystal_set_box_slot_round_trip);
    RUN_TEST(test_gs_set_box_slot_out_of_range);
    RUN_TEST(test_gs_get_items_pocket);
    RUN_TEST(test_gs_get_balls_pocket);
    RUN_TEST(test_gs_get_key_pocket);
    RUN_TEST(test_gs_set_item_round_trip);
    RUN_TEST(test_gs_set_item_out_of_range);
    RUN_TEST(test_crystal_get_items_pocket);
    RUN_TEST(test_crystal_set_item_round_trip);
    RUN_TEST(test_gen2_species_name_lookup);
    RUN_TEST(test_gen2_move_name_lookup);
    RUN_TEST(test_gen2_item_name_lookup);
    RUN_TEST(test_gen2_is_shiny_derived);
    RUN_TEST(test_gen2_stat_exp_passthrough);
    return UNITY_END();
}
