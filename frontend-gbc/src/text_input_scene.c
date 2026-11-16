/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <string.h>
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "text_input_charset.h"

#define GRID_COLS  5U
#define GRID_ROWS  6U
#define GRID_CELLS (GRID_COLS * GRID_ROWS)

static const char * const k_tab_names[3] = { "UPPER", "lower", "sym" };

static char         g_buf[13];
static uint8_t      g_buf_len     = 0U;
static uint8_t      g_max_len     = 7U;
static scene_kind_t g_return_to   = SCENE_MAIN_MENU;
static uint8_t      g_tab         = 0U;
static uint8_t      g_grid_cursor = 0U;

static const char *tab_chars(uint8_t t)
{
    switch (t) {
        case 0U: return PKSAVE_GBC_CHARSET_UPPER;
        case 1U: return PKSAVE_GBC_CHARSET_LOWER;
        case 2U: return PKSAVE_GBC_CHARSET_SYMBOL;
        default: return PKSAVE_GBC_CHARSET_UPPER;
    }
}

static uint8_t tab_len(uint8_t t)
{
    switch (t) {
        case 0U: return (uint8_t)(sizeof(PKSAVE_GBC_CHARSET_UPPER)  - 1U);
        case 1U: return (uint8_t)(sizeof(PKSAVE_GBC_CHARSET_LOWER)  - 1U);
        case 2U: return (uint8_t)(sizeof(PKSAVE_GBC_CHARSET_SYMBOL) - 1U);
        default: return 0U;
    }
}

static void text_input_redraw(void)
{
    char line[21];

    /* Header row */
    str_clear(line);
    str_append(line, sizeof(line), "NAME (");
    str_append(line, sizeof(line), k_tab_names[g_tab]);
    str_append(line, sizeof(line), ")");
    display_draw_header(line, 0U);

    /* Buffer display row: "  [RED_...]" */
    str_clear(line);
    str_append(line, sizeof(line), "  [");
    for (uint8_t i = 0U; i < g_buf_len; i++) {
        str_append_char(line, sizeof(line), g_buf[i]);
    }
    if (g_buf_len < g_max_len) {
        str_append_char(line, sizeof(line), '_');
    }
    for (uint8_t i = (uint8_t)(g_buf_len + 1U); i < g_max_len; i++) {
        str_append_char(line, sizeof(line), '.');
    }
    str_append_char(line, sizeof(line), ']');
    display_draw_row(2U, line, PAL_SUBHDR);

    /* 5x6 character grid on rows 4..15 (every 2 rows) */
    const char *chars = tab_chars(g_tab);
    uint8_t len       = tab_len(g_tab);

    for (uint8_t r = 0U; r < GRID_ROWS; r++) {
        uint8_t y = (uint8_t)(4U + r * 2U);
        uint8_t row_has_cursor = 0U;
        str_clear(line);
        str_append(line, sizeof(line), "  ");
        for (uint8_t c = 0U; c < GRID_COLS; c++) {
            uint8_t idx = (uint8_t)(r * GRID_COLS + c);
            char cell   = (idx < len) ? chars[idx] : ' ';
            if (idx == g_grid_cursor) {
                str_append_char(line, sizeof(line), '[');
                str_append_char(line, sizeof(line), cell);
                str_append_char(line, sizeof(line), ']');
                row_has_cursor = 1U;
            } else {
                str_append_char(line, sizeof(line), ' ');
                str_append_char(line, sizeof(line), cell);
                str_append_char(line, sizeof(line), ' ');
            }
        }
        display_draw_row(y, line, row_has_cursor ? PAL_CURSOR : PAL_BODY);
    }

    display_draw_footer(" A:Add B:Del ST:Done");
}

void text_input_scene_init(void)
{
    memset(g_buf, 0, sizeof(g_buf));
    g_max_len   = g_text_input.max_len;
    if (g_max_len == 0U || g_max_len > 12U) g_max_len = 7U;
    g_return_to = g_text_input.return_to;

    g_buf_len = 0U;
    while (g_buf_len < g_max_len && g_text_input.initial[g_buf_len] != '\0') {
        g_buf[g_buf_len] = g_text_input.initial[g_buf_len];
        g_buf_len++;
    }
    g_buf[g_buf_len] = '\0';

    g_tab         = 0U;
    g_grid_cursor = 0U;
    g_text_input.result_valid = 0U;

    display_clear(PAL_BODY);
    text_input_redraw();
}

static void move_grid_cursor(int8_t dx, int8_t dy)
{
    uint8_t len = tab_len(g_tab);
    if (len == 0U) return;

    int8_t col = (int8_t)(g_grid_cursor % GRID_COLS);
    int8_t row = (int8_t)(g_grid_cursor / GRID_COLS);

    col = (int8_t)((col + dx + (int8_t)GRID_COLS) % (int8_t)GRID_COLS);
    row = (int8_t)((row + dy + (int8_t)GRID_ROWS) % (int8_t)GRID_ROWS);

    uint8_t cand = (uint8_t)((uint8_t)row * GRID_COLS + (uint8_t)col);
    if (cand < len) {
        g_grid_cursor = cand;
    } else {
        g_grid_cursor = (uint8_t)(len - 1U);
    }
}

scene_kind_t text_input_scene_update(void)
{
    if (input_pressed(J_START)) {
        memset(g_text_input.result_value, 0, sizeof(g_text_input.result_value));
        for (uint8_t i = 0U; i < g_buf_len; i++) {
            g_text_input.result_value[i] = g_buf[i];
        }
        g_text_input.result_cancelled = 0U;
        g_text_input.result_valid     = 1U;
        g_text_input.result_tag       = g_text_input.caller_tag;
        return g_return_to;
    }

    if (input_pressed(J_SELECT)) {
        g_tab = (uint8_t)((g_tab + 1U) % 3U);
        uint8_t len = tab_len(g_tab);
        if (g_grid_cursor >= len) {
            g_grid_cursor = (len > 0U) ? (uint8_t)(len - 1U) : 0U;
        }
        text_input_redraw();
        return SCENE_NONE;
    }

    if (input_pressed(J_B)) {
        if (g_buf_len == 0U) {
            memset(g_text_input.result_value, 0, sizeof(g_text_input.result_value));
            g_text_input.result_cancelled = 1U;
            g_text_input.result_valid     = 1U;
            g_text_input.result_tag       = g_text_input.caller_tag;
            return g_return_to;
        }
        g_buf_len--;
        g_buf[g_buf_len] = '\0';
        text_input_redraw();
        return SCENE_NONE;
    }

    if (input_pressed(J_A)) {
        uint8_t len = tab_len(g_tab);
        if (g_buf_len < g_max_len && g_grid_cursor < len) {
            const char *chars = tab_chars(g_tab);
            g_buf[g_buf_len++] = chars[g_grid_cursor];
            g_buf[g_buf_len]   = '\0';
            text_input_redraw();
        }
        return SCENE_NONE;
    }

    uint8_t moved = 0U;
    if (input_pressed(J_LEFT))  { move_grid_cursor(-1,  0); moved = 1U; }
    if (input_pressed(J_RIGHT)) { move_grid_cursor( 1,  0); moved = 1U; }
    if (input_pressed(J_UP))    { move_grid_cursor( 0, -1); moved = 1U; }
    if (input_pressed(J_DOWN))  { move_grid_cursor( 0,  1); moved = 1U; }

    if (moved) {
        text_input_redraw();
    }

    return SCENE_NONE;
}
