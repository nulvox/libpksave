/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen 3 text-input keyboard character set.
 *
 * Shared between the C++ widget (text_input.cpp) and the Unity test
 * (libpksave/tests/test_gen3_charset.c) so they can never drift: every
 * character the keyboard exposes is asserted to round-trip cleanly
 * through libpksave's Gen 3 trainer encoder.
 *
 * The Gen 3 charset, per pret/pokeemerald charmap.txt:
 *   - ' '        → byte  0x00
 *   - '0'..'9'   → bytes 0xA1..0xAA
 *   - '!' '?' '.' '-' '"' '\'' ',' '/' → bytes 0xAB-0xBA (sparse subset)
 *   - 'A'..'Z'   → bytes 0xBB..0xD4
 *   - 'a'..'z'   → bytes 0xD5..0xEE
 *   - terminator → 0xFF
 *   - anything else → encodes to 0xFF (terminator).  Those characters do
 *     NOT round-trip and therefore are NOT exposed on the keyboard.
 *
 * This is a C header (works in both .c and .cpp translation units).
 */
#ifndef PKSAVE_GBA_TEXT_INPUT_CHARSET_H
#define PKSAVE_GBA_TEXT_INPUT_CHARSET_H

/* Uppercase tab: A-Z + space (27 cells; 5x6 grid leaves 3 blank). */
#define PKSAVE_GEN3_CHARSET_UPPER  "ABCDEFGHIJKLMNOPQRSTUVWXYZ "

/* Lowercase tab: a-z + space (27 cells; 5x6 grid leaves 3 blank). */
#define PKSAVE_GEN3_CHARSET_LOWER  "abcdefghijklmnopqrstuvwxyz "

/* Symbol tab: digits 0-9, the eight punctuation marks present in the
 * Gen 3 in-game name-entry keyboard (! ? . - , / ' "), and space.
 * Bytes per pret/pokeemerald charmap.txt punctuation block 0xAB-0xBA. */
#define PKSAVE_GEN3_CHARSET_SYMBOL "0123456789!?.-,/'\" "

#endif /* PKSAVE_GBA_TEXT_INPUT_CHARSET_H */
