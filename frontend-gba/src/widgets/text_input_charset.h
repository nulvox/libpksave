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
 * The Gen 3 charset, per src/gen3.c (gen3_encode_char / gen3_decode_char),
 * is:
 *   - 'A'..'Z'   → bytes 0xA1..0xBA
 *   - 'a'..'z'   → bytes 0xBB..0xD4
 *   - ' '        → byte  0x00
 *   - everything else → encodes to 0xFF, which the decoder treats as
 *     end-of-string.  Those characters do NOT round-trip and therefore
 *     are NOT exposed on the keyboard.
 *
 * That leaves digits and punctuation out of v1.  When libpksave grows
 * support for the rest of the Gen 3 in-game name-entry alphabet (digits,
 * apostrophe, hyphen, period — the real keyboard has them), this header
 * and the Unity test update together.  The "Symbol" tab is intentionally
 * sparse for now (just space) so the keyboard layout remains stable
 * once we expand.
 *
 * This is a C header (works in both .c and .cpp translation units).
 */
#ifndef PKSAVE_GBA_TEXT_INPUT_CHARSET_H
#define PKSAVE_GBA_TEXT_INPUT_CHARSET_H

/* Uppercase tab: A-Z + space (27 cells; 5x6 grid leaves 3 blank). */
#define PKSAVE_GEN3_CHARSET_UPPER  "ABCDEFGHIJKLMNOPQRSTUVWXYZ "

/* Lowercase tab: a-z + space (27 cells; 5x6 grid leaves 3 blank). */
#define PKSAVE_GEN3_CHARSET_LOWER  "abcdefghijklmnopqrstuvwxyz "

/* Symbol tab: only space round-trips through the current encoder.
 * Listed separately from the case tabs so the SELECT-cycles-tabs
 * affordance is preserved end-to-end, even when sparse. */
#define PKSAVE_GEN3_CHARSET_SYMBOL " "

#endif /* PKSAVE_GBA_TEXT_INPUT_CHARSET_H */
