/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Per-generation checksum validation helper (test helper only).
 * Actual flush/checksum writing is inside each genN.c.
 *
 * Gen I:   (~sum(0x2598..0x3522)) & 0xFF at 0x3523
 * Gen II:  16-bit sum stored LE; GS vs Crystal differ in range/location
 * Gen III: 16-bit folded 32-bit word sum per section footer
 * Gen IV:  CRC-16-CCITT per block
 * Gen V:   CRC-16-CCITT per block (same algorithm, different block sizes)
 */
#include "pksave_internal.h"

int pksave_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game)
{
    if (!sram) return PKSAVE_ERR_NULL;

    switch (game) {

#ifdef PKSAVE_GEN1
    case PKSAVE_GAME_RB:
    case PKSAVE_GAME_YELLOW:
        return gen1_validate_checksum(sram, len, game);
#endif

#ifdef PKSAVE_GEN2
    case PKSAVE_GAME_GS:
    case PKSAVE_GAME_CRYSTAL:
        return gen2_validate_checksum(sram, len, game);
#endif

#ifdef PKSAVE_GEN3
    case PKSAVE_GAME_RS:
    case PKSAVE_GAME_EMERALD:
    case PKSAVE_GAME_FRLG:
        return gen3_validate_checksum(sram, len, game);
#endif

#ifdef PKSAVE_GEN4
    case PKSAVE_GAME_DP:
    case PKSAVE_GAME_PLATINUM:
    case PKSAVE_GAME_HGSS:
        return gen4_validate_checksum(sram, len, game);
#endif

#ifdef PKSAVE_GEN5
    case PKSAVE_GAME_BW:
    case PKSAVE_GAME_BW2:
        return gen5_validate_checksum(sram, len, game);
#endif

    default:
        return PKSAVE_ERR_GAME;
    }
}
