/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * TODO(3): implement per-generation checksum algorithms.
 * Gen I:   single byte sum over save block (see pokered/src/engine/save.asm)
 * Gen II:  16-bit sum, two checksum regions (see pokecrystal/engine/save.asm)
 * Gen III: 16-bit word sum over each 4KB section (see pokeemerald/src/save.c)
 * Gen IV:  CRC-16-CCITT over each block (see pokeplatinum/src/save.c)
 * Gen V:   CRC-16-CCITT, different block layout (supplement with PKHeX)
 */
#include "pksave_internal.h"

int pksave_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game)
{
    if (!sram) return PKSAVE_ERR_NULL;
    (void)len;
    (void)game;
    return PKSAVE_ERR_GAME; /* TODO(3): implement */
}
