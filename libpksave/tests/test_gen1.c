/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen 1 tests — RED until TODO(3) implements gen1.c.
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

/* ── rb ── */
static uint8_t rb_buf[SRAM_SIZE];

void test_rb_trainer_round_trip(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
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

    s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_rb_party_slot0(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
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

void test_rb_checksum_valid_after_flush(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB));
}

void test_rb_party_slot_round_trip(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    party[0].species  = 25;   /* Pikachu (Gen I internal ID) */
    party[0].level    = 42;
    party[0].move_ids[0] = 85;  /* Thunderbolt */
    party[0].move_pp[0]  = 15;
    /* Stat exp passthrough: Gen I/II use 0-65535, not clamped */
    party[0].evs[0]   = 12345;
    /* DVs scaled 0-31 → encoded as 0-15 */
    party[0].ivs[1]   = 30;   /* Atk DV=15 */
    party[0].ivs[2]   = 28;   /* Def DV=14 */

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT16(25, party[0].species);
    TEST_ASSERT_EQUAL_UINT8(42, party[0].level);
    TEST_ASSERT_EQUAL_UINT8(85, party[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(15, party[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT16(12345, party[0].evs[0]);
    TEST_ASSERT_EQUAL_UINT8(30, party[0].ivs[1]);
    TEST_ASSERT_EQUAL_UINT8(28, party[0].ivs[2]);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB));
    pksave_close(s);
}

/* ── yellow ── */
static uint8_t yellow_buf[SRAM_SIZE];

void test_yellow_trainer_round_trip(void)
{
    if (!load_fixture("yellow.sav", yellow_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(yellow_buf, SRAM_SIZE, PKSAVE_GAME_YELLOW);
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

    s = pksave_open(yellow_buf, SRAM_SIZE, PKSAVE_GAME_YELLOW);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));
    TEST_ASSERT_EQUAL_UINT32(new_money, tr.money);
    pksave_close(s);
}

void test_yellow_party_slot0(void)
{
    if (!load_fixture("yellow.sav", yellow_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(yellow_buf, SRAM_SIZE, PKSAVE_GAME_YELLOW);
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

void test_yellow_checksum_valid_after_flush(void)
{
    if (!load_fixture("yellow.sav", yellow_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(yellow_buf, SRAM_SIZE, PKSAVE_GAME_YELLOW);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(yellow_buf, SRAM_SIZE, PKSAVE_GAME_YELLOW));
}

/* ── Gen I PC box read ── */

void test_rb_box_count(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT(12, pksave_get_box_count(s));
    pksave_close(s);
}

void test_rb_get_empty_current_box(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    /* Current box is 0; fixture stores no box mons → count=0 */
    pksave_pokemon_t box[30];
    uint8_t count = 99;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 0, box, &count));
    TEST_ASSERT_EQUAL_UINT8(0, count);
    pksave_close(s);
}

void test_rb_get_box_out_of_range(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_get_box(s, 12, box, &count));
    pksave_close(s);
}

void test_rb_get_stored_box_with_mon(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t box[30];
    uint8_t count = 0;
    /* Fixture populates box 1 (stored, not current) with one Bulbasaur lvl 7 */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x99, box[0].species);   /* Bulbasaur internal ID */
    TEST_ASSERT_EQUAL_UINT8(7, box[0].level);
    TEST_ASSERT_EQUAL_UINT8(33, box[0].move_ids[0]); /* Tackle */
    pksave_close(s);
}

void test_rb_set_stored_box_slot_round_trip(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t box[30];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);

    /* Mutate slot 0 in stored box 1 */
    box[0].species      = 0x15;   /* Pidgey internal ID */
    box[0].level        = 25;
    box[0].move_ids[0]  = 16;     /* Gust */
    box[0].move_pp[0]   = 35;
    box[0].ivs[1]       = 30;     /* Atk DV=15 */
    strcpy(box[0].nickname, "BIRDY");

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_box_slot(s, 1, 0, &box[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_box(s, 1, box, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(0x15, box[0].species);
    TEST_ASSERT_EQUAL_UINT8(25, box[0].level);
    TEST_ASSERT_EQUAL_UINT8(16, box[0].move_ids[0]);
    TEST_ASSERT_EQUAL_UINT8(35, box[0].move_pp[0]);
    TEST_ASSERT_EQUAL_UINT8(30, box[0].ivs[1]);
    TEST_ASSERT_EQUAL_STRING("BIRDY", box[0].nickname);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB));
    pksave_close(s);
}

void test_rb_set_box_slot_out_of_range(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokemon_t mon;
    memset(&mon, 0, sizeof(mon));
    mon.species = 1;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 12, 0, &mon));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_slot(s, 0, 30, &mon));
    pksave_close(s);
}

/* ── Gen I derived fields: is_shiny + stat exp passthrough ── */

void test_gen1_is_shiny_derived(void)
{
    /* Gen II shiny formula (applied to Gen I DVs):
     *   Def DV = 10, Spd DV = 10, Spc DV = 10, Atk DV ∈ {2,3,6,7,10,11,14,15}
     * Our IVs are stored as DV*2, so DV 10 → IV 20, DV 15 → IV 30. */
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
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

    s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT8(1, party[0].is_shiny);

    /* Non-shiny DV set: Atk=12 not in {2,3,6,7,10,11,14,15} → not shiny */
    party[0].ivs[1] = 24;   /* DV 12 */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT8(0, party[0].is_shiny);
    pksave_close(s);
}

void test_gen1_stat_exp_passthrough(void)
{
    /* Gen I stat exp is 0-65535 per stat — must not be clamped to 255. */
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);

    pksave_pokemon_t party[6];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_GREATER_THAN(0, (int)count);

    party[0].evs[0] = 65535;       /* HP stat exp max */
    party[0].evs[1] = 50000;       /* Atk */
    party[0].evs[5] = 12345;       /* Speed */
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_party_slot(s, 0, &party[0]));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_party(s, party, &count));
    TEST_ASSERT_EQUAL_UINT16(65535, party[0].evs[0]);
    TEST_ASSERT_EQUAL_UINT16(50000, party[0].evs[1]);
    TEST_ASSERT_EQUAL_UINT16(12345, party[0].evs[5]);
    pksave_close(s);
}

/* ── Gen I name lookups ── */

void test_gen1_species_name_lookup(void)
{
    /* Internal IDs per pret/pokered constants/pokemon_constants.asm */
    TEST_ASSERT_EQUAL_STRING("Rhydon",     pksave_species_name(PKSAVE_GAME_RB, 0x01));
    TEST_ASSERT_EQUAL_STRING("Bulbasaur",  pksave_species_name(PKSAVE_GAME_RB, 0x99));
    TEST_ASSERT_EQUAL_STRING("Charizard",  pksave_species_name(PKSAVE_GAME_RB, 0xB4));
    TEST_ASSERT_EQUAL_STRING("Pikachu",    pksave_species_name(PKSAVE_GAME_RB, 0x54));
    TEST_ASSERT_EQUAL_STRING("Mewtwo",     pksave_species_name(PKSAVE_GAME_RB, 0x83));
    TEST_ASSERT_EQUAL_STRING("Mew",        pksave_species_name(PKSAVE_GAME_RB, 0x15));
    TEST_ASSERT_EQUAL_STRING("Pidgey",     pksave_species_name(PKSAVE_GAME_RB, 0x24));
    TEST_ASSERT_EQUAL_STRING("Victreebel", pksave_species_name(PKSAVE_GAME_RB, 0xBE));
    /* Skipped slots → MissingNo */
    TEST_ASSERT_EQUAL_STRING("MissingNo.", pksave_species_name(PKSAVE_GAME_RB, 0x1F));
    /* Out-of-range or zero → "???" */
    TEST_ASSERT_EQUAL_STRING("???",        pksave_species_name(PKSAVE_GAME_RB, 0x00));
    TEST_ASSERT_EQUAL_STRING("???",        pksave_species_name(PKSAVE_GAME_RB, 0xFF));
}

void test_gen1_move_name_lookup(void)
{
    /* Move IDs 1-indexed; per pret/pokered data/moves/names.asm */
    TEST_ASSERT_EQUAL_STRING("Pound",       pksave_move_name(PKSAVE_GAME_RB, 1));
    TEST_ASSERT_EQUAL_STRING("Tackle",      pksave_move_name(PKSAVE_GAME_RB, 33));
    TEST_ASSERT_EQUAL_STRING("Thunderbolt", pksave_move_name(PKSAVE_GAME_RB, 85));
    TEST_ASSERT_EQUAL_STRING("Surf",        pksave_move_name(PKSAVE_GAME_RB, 57));
    TEST_ASSERT_EQUAL_STRING("Struggle",    pksave_move_name(PKSAVE_GAME_RB, 165));
    TEST_ASSERT_EQUAL_STRING("???",         pksave_move_name(PKSAVE_GAME_RB, 0));
    TEST_ASSERT_EQUAL_STRING("???",         pksave_move_name(PKSAVE_GAME_RB, 200));
}

void test_gen1_item_name_lookup(void)
{
    /* Item IDs 1-indexed; per pret/pokered data/items/names.asm */
    TEST_ASSERT_EQUAL_STRING("Master Ball", pksave_item_name(PKSAVE_GAME_RB, 0x01));
    TEST_ASSERT_EQUAL_STRING("Potion",      pksave_item_name(PKSAVE_GAME_RB, 0x14));
    TEST_ASSERT_EQUAL_STRING("Antidote",    pksave_item_name(PKSAVE_GAME_RB, 0x0B));
    TEST_ASSERT_EQUAL_STRING("Full Heal",   pksave_item_name(PKSAVE_GAME_RB, 0x34));
    TEST_ASSERT_EQUAL_STRING("Rare Candy",  pksave_item_name(PKSAVE_GAME_RB, 0x28));
    TEST_ASSERT_EQUAL_STRING("???",         pksave_item_name(PKSAVE_GAME_RB, 0));
    TEST_ASSERT_EQUAL_STRING("???",         pksave_item_name(PKSAVE_GAME_RB, 0xFF));
}

/* ── Gen I bag / items pocket ── */

void test_rb_get_items_pocket(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);

    pksave_item_t items[20];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(3, count);
    TEST_ASSERT_EQUAL_UINT16(0x01, items[0].item_id);   /* Master Ball */
    TEST_ASSERT_EQUAL_UINT16(1,    items[0].quantity);
    TEST_ASSERT_EQUAL_UINT16(0x14, items[1].item_id);   /* Potion */
    TEST_ASSERT_EQUAL_UINT16(10,   items[1].quantity);
    TEST_ASSERT_EQUAL_UINT16(0x0B, items[2].item_id);   /* Antidote */
    TEST_ASSERT_EQUAL_UINT16(5,    items[2].quantity);
    pksave_close(s);
}

void test_rb_other_pockets_empty(void)
{
    /* Gen I only has an ITEMS pocket. The other pocket IDs should report
     * 0 items rather than ERR_GAME, so frontends can iterate uniformly. */
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[1];
    uint8_t count = 99;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_BALLS, items, &count));
    TEST_ASSERT_EQUAL_UINT8(0, count);
    pksave_close(s);
}

void test_rb_set_item_round_trip(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);

    pksave_item_t new_item = { .item_id = 0x4A, .quantity = 99 }; /* Full Heal */
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_set_item(s, PKSAVE_POCKET_ITEMS, 1, &new_item));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t items[20];
    uint8_t count = 0;
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_get_pocket(s, PKSAVE_POCKET_ITEMS, items, &count));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(0x4A, items[1].item_id);
    TEST_ASSERT_EQUAL_UINT16(99,   items[1].quantity);
    TEST_ASSERT_EQUAL(PKSAVE_OK,
        pksave_validate_checksum(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB));
    pksave_close(s);
}

void test_rb_set_item_out_of_range(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    pksave_item_t item = { .item_id = 1, .quantity = 1 };
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE,
        pksave_set_item(s, PKSAVE_POCKET_ITEMS, 20, &item));
    pksave_close(s);
}

/* ── Pokerus rejected on Gen 1 (mechanic introduced in Gen 2) ── */
void test_rb_pokerus_unsupported(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    TEST_ASSERT_NOT_NULL(s);
    pksave_pokerus_t pkrs;
    TEST_ASSERT_EQUAL(PKSAVE_ERR_GAME, pksave_get_party_pokerus(s, 0, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_GAME, pksave_set_party_pokerus(s, 0, 1, 1));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_GAME, pksave_get_box_pokerus(s, 0, 0, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_GAME, pksave_set_box_pokerus(s, 0, 0, 1, 1));
    pksave_close(s);
}

/* ── Argument validation across pokerus API ── */
void test_pokerus_rejects_bad_inputs(void)
{
    if (!load_fixture("rb.sav", rb_buf, SRAM_SIZE)) return;
    pksave_save_t *s = pksave_open(rb_buf, SRAM_SIZE, PKSAVE_GAME_RB);
    pksave_pokerus_t pkrs;

    /* NULL save / out → ERR_NULL */
    TEST_ASSERT_EQUAL(PKSAVE_ERR_NULL, pksave_get_party_pokerus(NULL, 0, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_NULL, pksave_get_party_pokerus(s, 0, NULL));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_NULL, pksave_set_party_pokerus(NULL, 0, 0, 0));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_NULL, pksave_get_box_pokerus(NULL, 0, 0, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_NULL, pksave_get_box_pokerus(s, 0, 0, NULL));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_NULL, pksave_set_box_pokerus(NULL, 0, 0, 0, 0));

    /* Out-of-range slot/strain/days → ERR_RANGE */
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_get_party_pokerus(s, 6, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_party_pokerus(s, 6, 0, 0));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_party_pokerus(s, 0, 16, 0));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_party_pokerus(s, 0, 0, 16));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_get_box_pokerus(s, 0, 30, &pkrs));
    TEST_ASSERT_EQUAL(PKSAVE_ERR_RANGE, pksave_set_box_pokerus(s, 0, 30, 0, 0));
    pksave_close(s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rb_trainer_round_trip);
    RUN_TEST(test_rb_party_slot0);
    RUN_TEST(test_rb_checksum_valid_after_flush);
    RUN_TEST(test_rb_party_slot_round_trip);
    RUN_TEST(test_yellow_trainer_round_trip);
    RUN_TEST(test_yellow_party_slot0);
    RUN_TEST(test_yellow_checksum_valid_after_flush);
    RUN_TEST(test_rb_box_count);
    RUN_TEST(test_rb_get_empty_current_box);
    RUN_TEST(test_rb_get_box_out_of_range);
    RUN_TEST(test_rb_get_stored_box_with_mon);
    RUN_TEST(test_rb_set_stored_box_slot_round_trip);
    RUN_TEST(test_rb_set_box_slot_out_of_range);
    RUN_TEST(test_rb_get_items_pocket);
    RUN_TEST(test_rb_other_pockets_empty);
    RUN_TEST(test_rb_set_item_round_trip);
    RUN_TEST(test_rb_set_item_out_of_range);
    RUN_TEST(test_gen1_species_name_lookup);
    RUN_TEST(test_gen1_move_name_lookup);
    RUN_TEST(test_gen1_item_name_lookup);
    RUN_TEST(test_gen1_is_shiny_derived);
    RUN_TEST(test_gen1_stat_exp_passthrough);
    RUN_TEST(test_rb_pokerus_unsupported);
    RUN_TEST(test_pokerus_rejects_bad_inputs);
    return UNITY_END();
}
