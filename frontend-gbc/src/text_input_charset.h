/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * Gen I/II text-input keyboard character set.
 * Matches libpksave's gen1_encode_char / gen2_encode_char round-trip alphabet:
 *   - 'A'..'Z' → 0x80..0x99
 *   - 'a'..'z' → 0xA0..0xB9
 *   - '0'..'9' → 0xF6..0xFF
 *   - '\''     → 0xE0
 *   - '-'      → 0xE3
 *   - ' '      → 0x7F
 */
#ifndef PKSAVE_GBC_TEXT_INPUT_CHARSET_H
#define PKSAVE_GBC_TEXT_INPUT_CHARSET_H

#define PKSAVE_GBC_CHARSET_UPPER  "ABCDEFGHIJKLMNOPQRSTUVWXYZ "
#define PKSAVE_GBC_CHARSET_LOWER  "abcdefghijklmnopqrstuvwxyz "
#define PKSAVE_GBC_CHARSET_SYMBOL "0123456789'- "

#endif /* PKSAVE_GBC_TEXT_INPUT_CHARSET_H */
