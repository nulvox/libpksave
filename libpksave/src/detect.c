/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * pksave_detect: identify save file game variant from raw SRAM bytes.
 *
 * Detection strategy by generation:
 *   Gen I   (32KB): validate Gen I checksum (sum 0x2598-0x3522 → 0x3523)
 *   Gen II  (32KB): validate Gen II checksums (GS vs Crystal differ)
 *   Gen III (128KB): look for section signature 0x08012025 in section footers
 *   Gen IV  (512KB): read block size field in footer → DP/Platinum/HGSS
 *   Gen V   (512KB): same footer structure, different block sizes
 */
#include "pksave_internal.h"

/* ── shared sizes ────────────────────────────────────────────────────────── */
#define SIZE_GEN12      (32U  * 1024U)
#define SIZE_GEN3       (128U * 1024U)
#define SIZE_GEN45      (512U * 1024U)

/* ── Gen I ───────────────────────────────────────────────────────────────── */
#ifdef PKSAVE_GEN1
#define GEN1_CHKSUM_START  0x2598U
#define GEN1_CHKSUM_END    0x3522U
#define GEN1_CHKSUM_ADDR   0x3523U
#define GEN1_YELLOW_MARKER 0x2720U   /* non-zero → Yellow; zero → R/B */

static int gen1_checksum_valid(const uint8_t *sram)
{
    uint8_t sum = 0;
    for (uint32_t i = GEN1_CHKSUM_START; i <= GEN1_CHKSUM_END; i++)
        sum += sram[i];
    /* checksum = (~sum) & 0xFF */
    return ((uint8_t)(~sum)) == sram[GEN1_CHKSUM_ADDR];
}

static pksave_game_t detect_gen1(const uint8_t *sram, size_t len)
{
    if (len < SIZE_GEN12) return PKSAVE_GAME_UNKNOWN;
    if (!gen1_checksum_valid(sram)) return PKSAVE_GAME_UNKNOWN;
    return (sram[GEN1_YELLOW_MARKER] != 0) ? PKSAVE_GAME_YELLOW : PKSAVE_GAME_RB;
}
#endif /* PKSAVE_GEN1 */

/* ── Gen II ──────────────────────────────────────────────────────────────── */
#ifdef PKSAVE_GEN2
/* GS primary:      sum(0x2009..0x2D68) stored LE at 0x2D69            */
/* Crystal primary: sum(0x2009..0x2B82) stored LE at 0x2D0D            */
/* Crystal secondary: sum(0x1209..0x1D82) stored LE at 0x1F0D           */

static uint16_t sum16(const uint8_t *buf, uint32_t start, uint32_t end)
{
    uint32_t s = 0;
    for (uint32_t i = start; i <= end; i++) s += buf[i];
    return (uint16_t)(s & 0xFFFF);
}

static uint16_t read_le16(const uint8_t *buf, uint32_t off)
{
    return (uint16_t)(buf[off] | ((uint16_t)buf[off+1] << 8));
}

static int crystal_checksum_valid(const uint8_t *sram)
{
    /* primary: 0x2009-0x2B82 at 0x2D0D */
    if (sum16(sram, 0x2009, 0x2B82) != read_le16(sram, 0x2D0D))
        return 0;
    /* secondary: 0x1209-0x1D82 at 0x1F0D */
    if (sum16(sram, 0x1209, 0x1D82) != read_le16(sram, 0x1F0D))
        return 0;
    return 1;
}

static int gs_checksum_valid(const uint8_t *sram)
{
    return sum16(sram, 0x2009, 0x2D68) == read_le16(sram, 0x2D69);
}

static pksave_game_t detect_gen2(const uint8_t *sram, size_t len)
{
    if (len < SIZE_GEN12) return PKSAVE_GAME_UNKNOWN;
    if (crystal_checksum_valid(sram)) return PKSAVE_GAME_CRYSTAL;
    if (gs_checksum_valid(sram))      return PKSAVE_GAME_GS;
    return PKSAVE_GAME_UNKNOWN;
}
#endif /* PKSAVE_GEN2 */

/* ── Gen III ─────────────────────────────────────────────────────────────── */
#ifdef PKSAVE_GEN3
#define GEN3_SECTION_SIZE  0x1000U
#define GEN3_SIG           0x08012025UL
#define GEN3_SIG_OFF       0x0FF8U

/* Find the section with a given ID in the active slot.
 * Returns the byte offset of that section within sram, or SIZE_MAX. */
static size_t gen3_find_section(const uint8_t *sram, uint16_t target_id,
                                 uint32_t slot_offset)
{
    for (int s = 0; s < 14; s++) {
        size_t off = slot_offset + (size_t)s * GEN3_SECTION_SIZE;
        uint32_t sig;
        memcpy(&sig, sram + off + GEN3_SIG_OFF, 4);
        if (sig != GEN3_SIG) continue;
        uint16_t id;
        memcpy(&id, sram + off + 0xFF4, 2);
        if (id == target_id) return off;
    }
    return SIZE_MAX;
}

/* Return save count from any valid section's footer. */
static uint32_t gen3_slot_count(const uint8_t *sram, uint32_t slot_offset)
{
    for (int s = 0; s < 14; s++) {
        size_t off = slot_offset + (size_t)s * GEN3_SECTION_SIZE;
        uint32_t sig;
        memcpy(&sig, sram + off + GEN3_SIG_OFF, 4);
        if (sig == GEN3_SIG) {
            uint32_t cnt;
            memcpy(&cnt, sram + off + 0xFFC, 4);
            return cnt;
        }
    }
    return 0;
}

static pksave_game_t detect_gen3(const uint8_t *sram, size_t len)
{
    if (len < SIZE_GEN3) return PKSAVE_GAME_UNKNOWN;

    /* Pick active slot (higher save count). */
    uint32_t cnt0 = gen3_slot_count(sram, 0);
    uint32_t cnt1 = gen3_slot_count(sram, 14 * GEN3_SECTION_SIZE);
    uint32_t slot_off = (cnt1 > cnt0) ? (14 * GEN3_SECTION_SIZE) : 0;

    /* Verify at least one section has the right signature. */
    int found = 0;
    for (int s = 0; s < 14; s++) {
        size_t off = slot_off + (size_t)s * GEN3_SECTION_SIZE;
        uint32_t sig;
        memcpy(&sig, sram + off + GEN3_SIG_OFF, 4);
        if (sig == GEN3_SIG) { found = 1; break; }
    }
    if (!found) return PKSAVE_GAME_UNKNOWN;

    /* Read section 0 to get the game code. */
    size_t sec0 = gen3_find_section(sram, 0, slot_off);
    if (sec0 == SIZE_MAX) return PKSAVE_GAME_UNKNOWN;

    uint32_t game_code;
    memcpy(&game_code, sram + sec0 + 0x00AC, 4);

    if (game_code == 1) return PKSAVE_GAME_FRLG;    /* FRLG game code */
    if (game_code != 0) return PKSAVE_GAME_EMERALD;  /* Emerald security key */
    return PKSAVE_GAME_RS;
}
#endif /* PKSAVE_GEN3 */

/* ── Gen IV / V ──────────────────────────────────────────────────────────── */
#if defined(PKSAVE_GEN4) || defined(PKSAVE_GEN5)
/* Footer is the last 0x14 bytes of the block.
 * block_size field at footer offset +0x08.
 * CRC at footer offset +0x12. */

#define GEN4_DP_GENERAL_SIZE   0xC100UL
#define GEN4_PT_GENERAL_SIZE   0xCF2CUL
#define GEN4_HGSS_GENERAL_SIZE 0xF700UL
#define GEN5_BW_GENERAL_SIZE   0x26A00UL
#define GEN5_BW2_GENERAL_SIZE  0x26E00UL

static uint32_t read_le32(const uint8_t *buf, uint32_t off)
{
    return (uint32_t)buf[off]
         | ((uint32_t)buf[off+1] << 8)
         | ((uint32_t)buf[off+2] << 16)
         | ((uint32_t)buf[off+3] << 24);
}

/* Read the block_size field from the footer of the block starting at
 * slot_off.  We probe each candidate size's footer position. */
static uint32_t gen45_read_block_size(const uint8_t *sram, uint32_t slot_off,
                                       size_t sram_len,
                                       uint32_t candidate_size)
{
    if (slot_off + candidate_size > sram_len) return 0;
    uint32_t footer = slot_off + candidate_size - 0x14U;
    return read_le32(sram, footer + 0x08);
}

static pksave_game_t detect_gen45(const uint8_t *sram, size_t len)
{
    if (len < SIZE_GEN45) return PKSAVE_GAME_UNKNOWN;

    /* Try each known general-block size at slot 0 and see if the block_size
     * field in the footer equals that candidate size. */
    static const struct { uint32_t sz; pksave_game_t game; } candidates[] = {
#ifdef PKSAVE_GEN4
        { GEN4_DP_GENERAL_SIZE,   PKSAVE_GAME_DP       },
        { GEN4_PT_GENERAL_SIZE,   PKSAVE_GAME_PLATINUM },
        { GEN4_HGSS_GENERAL_SIZE, PKSAVE_GAME_HGSS     },
#endif
#ifdef PKSAVE_GEN5
        { GEN5_BW_GENERAL_SIZE,   PKSAVE_GAME_BW       },
        { GEN5_BW2_GENERAL_SIZE,  PKSAVE_GAME_BW2      },
#endif
    };

    for (size_t i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
        uint32_t sz = candidates[i].sz;
        /* Check slot 0 */
        uint32_t reported = gen45_read_block_size(sram, 0, len, sz);
        if (reported == sz) return candidates[i].game;
        /* Check slot 1 (at offset 0x40000) */
        reported = gen45_read_block_size(sram, 0x40000U, len, sz);
        if (reported == sz) return candidates[i].game;
    }
    return PKSAVE_GAME_UNKNOWN;
}
#endif /* GEN4 || GEN5 */

/* ── public entry point ──────────────────────────────────────────────────── */
pksave_game_t pksave_detect(const uint8_t *sram, size_t len)
{
    if (!sram) return PKSAVE_GAME_UNKNOWN;

    if (len == SIZE_GEN12) {
        pksave_game_t g;
#ifdef PKSAVE_GEN1
        g = detect_gen1(sram, len);
        if (g != PKSAVE_GAME_UNKNOWN) return g;
#endif
#ifdef PKSAVE_GEN2
        g = detect_gen2(sram, len);
        if (g != PKSAVE_GAME_UNKNOWN) return g;
#endif
        (void)0;
    }

    if (len == SIZE_GEN3) {
#ifdef PKSAVE_GEN3
        return detect_gen3(sram, len);
#endif
    }

    if (len == SIZE_GEN45) {
#if defined(PKSAVE_GEN4) || defined(PKSAVE_GEN5)
        return detect_gen45(sram, len);
#endif
    }

    return PKSAVE_GAME_UNKNOWN;
}
