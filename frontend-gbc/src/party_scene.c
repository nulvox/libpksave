/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <string.h>
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"

#define PARTY_SLOT_COUNT 6U

static uint8_t     g_party_count = 0U;
static list_view_t g_list;

static void party_format_row(uint8_t row, char *out, uint8_t max_len)
{
    str_clear(out);
    str_append_u32(out, max_len, (uint32_t)(row + 1U));
    str_append(out, max_len, ". ");

    if (row >= g_party_count || g_party_buf[row].species == 0U) {
        str_append(out, max_len, "[Empty]");
        return;
    }

    const char *sp = bridge_species_name(g_party_buf[row].species);
    str_append(out, max_len, sp);
    str_append(out, max_len, " L");
    str_append_u32(out, max_len, (uint32_t)g_party_buf[row].level);
}

static void party_redraw(void)
{
    char row_buf[21];
    char line[21];

    display_draw_header("PARTY", bridge_is_dirty());
    display_draw_subheader(" SELECT POKEMON");
    display_clear_rows(2U, 16U, PAL_BODY);

    for (uint8_t i = 0U; i < PARTY_SLOT_COUNT; i++) {
        uint8_t y = (uint8_t)(3U + i * 2U);
        uint8_t selected = (i == g_list.selected);
        party_format_row(i, row_buf, sizeof(row_buf));
        str_clear(line);
        str_append(line, sizeof(line), selected ? "> " : "  ");
        str_append(line, sizeof(line), row_buf);
        display_draw_row(y, line, selected ? PAL_CURSOR : PAL_BODY);
    }

    display_draw_footer(" U/D:Move A:Edit B:Bk");
}

void party_scene_init(void)
{
    bridge_clear_pending_box();
    memset(g_party_buf, 0, sizeof(g_party_buf));
    g_party_count = 0U;
    if (bridge_ok()) {
        bridge_get_party(g_party_buf, &g_party_count);
    }

    list_view_init(&g_list, PARTY_SLOT_COUNT, PARTY_SLOT_COUNT);
    int8_t prev = bridge_pending_party_slot();
    if (prev >= 0 && (uint8_t)prev < PARTY_SLOT_COUNT) {
        g_list.selected = (uint8_t)prev;
    }

    display_clear(PAL_BODY);
    party_redraw();
}

scene_kind_t party_scene_update(void)
{
    if (input_pressed(J_B)) {
        bridge_set_pending_party_slot(-1);
        return SCENE_MAIN_MENU;
    }

    if (list_view_tick(&g_list, 0U)) {
        party_redraw();
    }

    if (input_pressed(J_A)) {
        bridge_set_pending_party_slot((int8_t)g_list.selected);
        return SCENE_POKEMON_EDIT;
    }

    return SCENE_NONE;
}
