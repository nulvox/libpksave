/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Round-trip every character the frontend-gba text_input keyboard
 * exposes through pksave_set_trainer / pksave_get_trainer.  This guards
 * the invariant: the keyboard never offers a character libpksave can't
 * encode-then-decode cleanly back to the same ASCII byte.
 *
 * The charset list lives in frontend-gba/src/widgets/text_input_charset.h
 * — shared between the C++ widget and this C test so they can never
 * drift.  If you add or remove a character there, this test re-runs the
 * proof.
 *
 * Round-trip strategy: write a 7-character name made of [c, c, c, c,
 * c, c, c] (PKSAVE_GEN3_TRAINER_NAME_MAX = 7 fits exactly), flush,
 * re-read, assert byte-for-byte equality.  Done once per character per
 * fixture (rs).  Other Gen III fixtures use the same encoder so once is
 * enough — checking rs covers the codepath.
 */
#include "unity.h"
#include "pksave.h"
#include "text_input_charset.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define SRAM_SIZE (128 * 1024)
#define FIXTURE_DIR "tests/fixtures/"

static uint8_t buf[SRAM_SIZE];

static int load_fixture(const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), FIXTURE_DIR "%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) { printf("SKIP: fixture not found: %s\n", path); return 0; }
    size_t n = fread(buf, 1, SRAM_SIZE, f);
    fclose(f);
    if (n != SRAM_SIZE) {
        printf("SKIP: fixture wrong size: %s\n", path);
        return 0;
    }
    return 1;
}

void setUp(void)    {}
void tearDown(void) {}

/* Round-trip a single character through the trainer-name field. */
static void round_trip_char(char c)
{
    if (!load_fixture("rs.sav")) return;

    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL_MESSAGE(s, "pksave_open failed");

    pksave_trainer_t tr;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));

    /* Fill the name with the test character, exactly TRAINER_NAME_MAX
     * chars so we exercise the full encoded width.  Buffer is 13 bytes
     * incl null terminator; libpksave will read up to 7 chars. */
    memset(tr.name, 0, sizeof(tr.name));
    for (int i = 0; i < PKSAVE_GEN3_TRAINER_NAME_MAX; i++) {
        tr.name[i] = c;
    }

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_trainer(s, &tr));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    /* Re-open, re-read, assert the bytes survived. */
    s = pksave_open(buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL(s);
    pksave_trainer_t tr2;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr2));

    char expected[PKSAVE_GEN3_TRAINER_NAME_MAX + 1];
    memset(expected, c, PKSAVE_GEN3_TRAINER_NAME_MAX);
    expected[PKSAVE_GEN3_TRAINER_NAME_MAX] = '\0';

    /* Build a helpful message including the offending character. */
    char msg[64];
    snprintf(msg, sizeof(msg),
        "char 0x%02X ('%c') did not round-trip", (unsigned)(unsigned char)c, c);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, tr2.name, msg);

    pksave_close(s);
}

void test_every_keyboard_char_round_trips(void)
{
    /* Iterate the union of all three keyboard tabs.  Duplicates across
     * tabs (e.g. space appearing on multiple) are harmless — the
     * round-trip is idempotent. */
    static const char *const tabs[] = {
        PKSAVE_GEN3_CHARSET_UPPER,
        PKSAVE_GEN3_CHARSET_LOWER,
        PKSAVE_GEN3_CHARSET_SYMBOL,
    };
    for (size_t t = 0; t < sizeof(tabs) / sizeof(tabs[0]); t++) {
        const char *s = tabs[t];
        for (size_t i = 0; s[i] != '\0'; i++) {
            round_trip_char(s[i]);
        }
    }
}

void test_charset_constants_non_empty(void)
{
    /* Sanity check: each tab has at least one character.  Catches
     * mis-typed header that compiles but yields empty strings. */
    TEST_ASSERT_TRUE(PKSAVE_GEN3_CHARSET_UPPER[0]  != '\0');
    TEST_ASSERT_TRUE(PKSAVE_GEN3_CHARSET_LOWER[0]  != '\0');
    TEST_ASSERT_TRUE(PKSAVE_GEN3_CHARSET_SYMBOL[0] != '\0');
}

/* Encoder canonicality check.
 *
 * Self-consistent round-trip (the other charset test) only proves the
 * encoder and decoder are inverses of each other.  It does NOT prove the
 * bytes on disk are what an actual Pokemon Gen III cartridge would render.
 * The cartridge's charmap (per pret/pokeemerald charmap.txt) is fixed:
 *   'A' = 0xBB, 'b' = 0xD6, 'Y' = 0xD3, 'z' = 0xEE, terminator = 0xFF.
 *
 * This test writes a known trainer name through the public API and then
 * scans the SRAM buffer for that canonical sequence.  If found, the
 * encoder is shipping bytes the real game can read.  If not, names
 * round-trip inside libpksave but render as gibberish on a real cart.
 *
 * The 5-byte sequence (0xBB 0xD6 0xD3 0xEE 0xFF) is distinctive enough
 * that it won't collide with unrelated fixture data. */
void test_gen3_encoder_canonical_bytes(void)
{
    if (!load_fixture("rs.sav")) return;

    pksave_save_t *s = pksave_open(buf, SRAM_SIZE, PKSAVE_GAME_RS);
    TEST_ASSERT_NOT_NULL_MESSAGE(s, "pksave_open failed");

    pksave_trainer_t tr;
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_get_trainer(s, &tr));

    /* Four-char name with mixed case; the cartridge truncates at the
     * first 0xFF so byte 5 should be the terminator. */
    memset(tr.name, 0, sizeof(tr.name));
    strcpy(tr.name, "AbYz");

    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_set_trainer(s, &tr));
    TEST_ASSERT_EQUAL(PKSAVE_OK, pksave_flush(s));
    pksave_close(s);

    /* Canonical Gen 3 (pret/pokeemerald charmap.txt):
     *   'A' 0xBB, 'b' 0xD6, 'Y' 0xD3, 'z' 0xEE, end-of-string 0xFF. */
    static const uint8_t canonical[] = { 0xBB, 0xD6, 0xD3, 0xEE, 0xFF };
    int found = 0;
    for (size_t i = 0; i + sizeof(canonical) <= SRAM_SIZE; i++) {
        if (memcmp(buf + i, canonical, sizeof(canonical)) == 0) {
            found = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found,
        "encoded \"AbYz\" not present at canonical Gen 3 bytes "
        "(BB D6 D3 EE FF) anywhere in SRAM — encoder is not "
        "pret-canonical and saves won't render correctly on a real cart");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_charset_constants_non_empty);
    RUN_TEST(test_every_keyboard_char_round_trips);
    RUN_TEST(test_gen3_encoder_canonical_bytes);
    return UNITY_END();
}
