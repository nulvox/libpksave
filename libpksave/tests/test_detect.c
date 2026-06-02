/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Detection tests — RED until TODO(3) is implemented.
 *
 * Fixture files are loaded from tests/fixtures/.
 * If a fixture is missing, the test is skipped with a message.
 */
#include "unity.h"
#include "pksave.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ── fixture helpers ─────────────────────────────────────────────────────── */

#define GEN1_SRAM_SIZE  (32  * 1024)
#define GEN2_SRAM_SIZE  (32  * 1024)
#define GEN3_SRAM_SIZE  (128 * 1024)
#define GEN4_SRAM_SIZE  (512 * 1024)
#define GEN5_SRAM_SIZE  (512 * 1024)

#define FIXTURE_DIR "tests/fixtures/"

static int load_fixture(const char *name, uint8_t *buf, size_t len)
{
    char path[256];
    snprintf(path, sizeof(path), FIXTURE_DIR "%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("SKIP: fixture not found: %s\n", path);
        return 0;
    }
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    if (n != len) {
        printf("SKIP: fixture wrong size: %s (got %zu, want %zu)\n",
               path, n, len);
        return 0;
    }
    return 1;
}

/* ── test buffers ────────────────────────────────────────────────────────── */
static uint8_t rb_buf[GEN1_SRAM_SIZE];
static uint8_t yellow_buf[GEN1_SRAM_SIZE];
static uint8_t gs_buf[GEN2_SRAM_SIZE];
static uint8_t crystal_buf[GEN2_SRAM_SIZE];
static uint8_t rs_buf[GEN3_SRAM_SIZE];
static uint8_t emerald_buf[GEN3_SRAM_SIZE];
static uint8_t frlg_buf[GEN3_SRAM_SIZE];
static uint8_t dp_buf[GEN4_SRAM_SIZE];
static uint8_t platinum_buf[GEN4_SRAM_SIZE];
static uint8_t hgss_buf[GEN4_SRAM_SIZE];
static uint8_t bw_buf[GEN5_SRAM_SIZE];
static uint8_t bw2_buf[GEN5_SRAM_SIZE];

void setUp(void)    {}
void tearDown(void) {}

/* ── detect returns UNKNOWN for NULL ─────────────────────────────────────── */
void test_detect_null_returns_unknown(void)
{
    TEST_ASSERT_EQUAL(PKSAVE_GAME_UNKNOWN, pksave_detect(NULL, 32768));
}

/* ── Gen I ───────────────────────────────────────────────────────────────── */
void test_detect_rb(void)
{
    if (!load_fixture("rb.sav", rb_buf, sizeof(rb_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_RB, pksave_detect(rb_buf, sizeof(rb_buf)));
}

void test_detect_yellow(void)
{
    if (!load_fixture("yellow.sav", yellow_buf, sizeof(yellow_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_YELLOW,
                      pksave_detect(yellow_buf, sizeof(yellow_buf)));
}

/* ── Gen II ──────────────────────────────────────────────────────────────── */
void test_detect_gs(void)
{
    if (!load_fixture("gs.sav", gs_buf, sizeof(gs_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_GS, pksave_detect(gs_buf, sizeof(gs_buf)));
}

void test_detect_crystal(void)
{
    if (!load_fixture("crystal.sav", crystal_buf, sizeof(crystal_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_CRYSTAL,
                      pksave_detect(crystal_buf, sizeof(crystal_buf)));
}

/* ── Gen III ─────────────────────────────────────────────────────────────── */
void test_detect_rs(void)
{
    if (!load_fixture("rs.sav", rs_buf, sizeof(rs_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_RS, pksave_detect(rs_buf, sizeof(rs_buf)));
}

void test_detect_emerald(void)
{
    if (!load_fixture("emerald.sav", emerald_buf, sizeof(emerald_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_EMERALD,
                      pksave_detect(emerald_buf, sizeof(emerald_buf)));
}

void test_detect_frlg(void)
{
    if (!load_fixture("frlg.sav", frlg_buf, sizeof(frlg_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_FRLG,
                      pksave_detect(frlg_buf, sizeof(frlg_buf)));
}

/* ── Gen IV ──────────────────────────────────────────────────────────────── */
void test_detect_dp(void)
{
    if (!load_fixture("dp.sav", dp_buf, sizeof(dp_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_DP, pksave_detect(dp_buf, sizeof(dp_buf)));
}

void test_detect_platinum(void)
{
    if (!load_fixture("platinum.sav", platinum_buf, sizeof(platinum_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_PLATINUM,
                      pksave_detect(platinum_buf, sizeof(platinum_buf)));
}

void test_detect_hgss(void)
{
    if (!load_fixture("hgss.sav", hgss_buf, sizeof(hgss_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_HGSS,
                      pksave_detect(hgss_buf, sizeof(hgss_buf)));
}

/* ── Gen V ───────────────────────────────────────────────────────────────── */
void test_detect_bw(void)
{
    if (!load_fixture("bw.sav", bw_buf, sizeof(bw_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_BW, pksave_detect(bw_buf, sizeof(bw_buf)));
}

void test_detect_bw2(void)
{
    if (!load_fixture("bw2.sav", bw2_buf, sizeof(bw2_buf))) return;
    TEST_ASSERT_EQUAL(PKSAVE_GAME_BW2,
                      pksave_detect(bw2_buf, sizeof(bw2_buf)));
}

/* ── runner ──────────────────────────────────────────────────────────────── */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_detect_null_returns_unknown);
    RUN_TEST(test_detect_rb);
    RUN_TEST(test_detect_yellow);
    RUN_TEST(test_detect_gs);
    RUN_TEST(test_detect_crystal);
    RUN_TEST(test_detect_rs);
    RUN_TEST(test_detect_emerald);
    RUN_TEST(test_detect_frlg);
    RUN_TEST(test_detect_dp);
    RUN_TEST(test_detect_platinum);
    RUN_TEST(test_detect_hgss);
    RUN_TEST(test_detect_bw);
    RUN_TEST(test_detect_bw2);
    return UNITY_END();
}
