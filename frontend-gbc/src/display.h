/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * GBC tilemap display and string-formatting helpers (160x144 = 20x18 tiles).
 */
#ifndef PKSAVE_GBC_DISPLAY_H
#define PKSAVE_GBC_DISPLAY_H

#include <stdint.h>

#define SCREEN_COLS 20U
#define SCREEN_ROWS 18U

/* GBC background palette indices */
#define PAL_BODY    0U
#define PAL_HEADER  1U
#define PAL_CURSOR  2U
#define PAL_SUBHDR  3U
#define PAL_ERROR   4U
#define PAL_EDIT    5U

/* Special icon character mapped to tile 95 (0x7F - 0x20) */
#define CHAR_POKEBALL '\x7F'

void display_init(void);
void display_clear(uint8_t pal);
void display_clear_rows(uint8_t y_start, uint8_t y_end, uint8_t pal);
void display_draw_row(uint8_t y, const char *text, uint8_t pal);
void display_draw_header(const char *title, uint8_t is_dirty);
void display_draw_subheader(const char *text);
void display_draw_footer(const char *hint);

/* Lightweight string-formatting helpers (avoid pulling SDCC's printf). */
void str_clear(char *dst);
void str_append(char *dst, uint8_t max_len, const char *src);
void str_append_char(char *dst, uint8_t max_len, char c);
void str_append_u32(char *dst, uint8_t max_len, uint32_t val);
void str_append_u16_pad(char *dst, uint8_t max_len, uint16_t val,
                        uint8_t width, char pad);

#endif /* PKSAVE_GBC_DISPLAY_H */
