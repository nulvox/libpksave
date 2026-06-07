/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Fixture on-disk-size regression guard.
 *
 * Why this exists: generate_fixtures.py used to build Gen 3 saves via a
 * slice-assignment whose RHS was 9362 bytes wide instead of 4096, silently
 * growing each 128 KiB save to ~272 KiB. The C/Python loaders papered over
 * it by truncating to the canonical size at read time, so unit tests
 * passed even though the on-disk artifact was malformed — a footgun for
 * any consumer that loads the file whole (e.g. mGBA's save-load API in
 * the Lua POC).
 *
 * This test fseeks each fixture to EOF and asserts the byte count matches
 * the canonical SRAM size for its generation. If somebody reintroduces a
 * grow-the-bytearray bug in the generator, this turns red.
 */
#include "unity.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define GEN1_SRAM_SIZE  (32  * 1024)
#define GEN2_SRAM_SIZE  (32  * 1024)
#define GEN3_SRAM_SIZE  (128 * 1024)
#define GEN4_SRAM_SIZE  (512 * 1024)
#define GEN5_SRAM_SIZE  (512 * 1024)

#define FIXTURE_DIR "tests/fixtures/"

void setUp(void)    {}
void tearDown(void) {}

static void assert_fixture_size(const char *name, long expected)
{
    char path[256];
    snprintf(path, sizeof(path), FIXTURE_DIR "%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("SKIP: fixture not found: %s\n", path);
        TEST_IGNORE();
        return;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        TEST_FAIL_MESSAGE("fseek failed");
        return;
    }
    long actual = ftell(f);
    fclose(f);
    if (actual != expected) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "%s: on-disk size %ld != canonical %ld "
                 "(generator likely grew its bytearray via mismatched "
                 "slice-assign; see generate_fixtures.py)",
                 name, actual, expected);
        TEST_FAIL_MESSAGE(msg);
    }
}

/* ── Gen I ───────────────────────────────────────────────────────────────── */
void test_fixture_size_rb(void)      { assert_fixture_size("rb.sav",      GEN1_SRAM_SIZE); }
void test_fixture_size_yellow(void)  { assert_fixture_size("yellow.sav",  GEN1_SRAM_SIZE); }

/* ── Gen II ──────────────────────────────────────────────────────────────── */
void test_fixture_size_gs(void)      { assert_fixture_size("gs.sav",      GEN2_SRAM_SIZE); }
void test_fixture_size_crystal(void) { assert_fixture_size("crystal.sav", GEN2_SRAM_SIZE); }

/* ── Gen III ─────────────────────────────────────────────────────────────── */
void test_fixture_size_rs(void)      { assert_fixture_size("rs.sav",      GEN3_SRAM_SIZE); }
void test_fixture_size_emerald(void) { assert_fixture_size("emerald.sav", GEN3_SRAM_SIZE); }
void test_fixture_size_frlg(void)    { assert_fixture_size("frlg.sav",    GEN3_SRAM_SIZE); }

/* ── Gen IV ──────────────────────────────────────────────────────────────── */
void test_fixture_size_dp(void)       { assert_fixture_size("dp.sav",       GEN4_SRAM_SIZE); }
void test_fixture_size_platinum(void) { assert_fixture_size("platinum.sav", GEN4_SRAM_SIZE); }
void test_fixture_size_hgss(void)     { assert_fixture_size("hgss.sav",     GEN4_SRAM_SIZE); }

/* ── Gen V ───────────────────────────────────────────────────────────────── */
void test_fixture_size_bw(void)  { assert_fixture_size("bw.sav",  GEN5_SRAM_SIZE); }
void test_fixture_size_bw2(void) { assert_fixture_size("bw2.sav", GEN5_SRAM_SIZE); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fixture_size_rb);
    RUN_TEST(test_fixture_size_yellow);
    RUN_TEST(test_fixture_size_gs);
    RUN_TEST(test_fixture_size_crystal);
    RUN_TEST(test_fixture_size_rs);
    RUN_TEST(test_fixture_size_emerald);
    RUN_TEST(test_fixture_size_frlg);
    RUN_TEST(test_fixture_size_dp);
    RUN_TEST(test_fixture_size_platinum);
    RUN_TEST(test_fixture_size_hgss);
    RUN_TEST(test_fixture_size_bw);
    RUN_TEST(test_fixture_size_bw2);
    return UNITY_END();
}
