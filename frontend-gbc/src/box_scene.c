/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <string.h>
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"

#define BOX_SLOT_COUNT 20U
#define VIEWPORT_ROWS  14U

static uint8_t     g_box_index = 0U;
static uint8_t     g_box_total = 12U;
static uint8_t     g_box_count = 0U;
static list_view_t g_list;

static void box_load(void)
{
    memset(g_box_buf, 0, sizeof(g_box_buf));
    g_box_count = 0U;
    if (bridge_ok()) {
        bridge_get_box(g_box_index, g_box_buf, &g_box_count);
    }
}

static void box_redraw_header(void)
{
    char title[20];
    str_clear(title);
    str_append(title, sizeof(title), "BOX ");
    str_append_u32(title, sizeof(title), (uint32_t)(g_box_index + 1U));
    str_append(title, sizeof(title), " (");
    str_append_u32(title, sizeof(title), (uint32_t)g_box_count);
    str_append(title, sizeof(title), "/20)");
    display_draw_header(title, bridge_is_dirty());
    display_draw_subheader(" SEL:Next Box  A:Edit");
}

static void box_format_row(uint8_t row, char *out, uint8_t max_len)
{
    str_clear(out);
    str_append_u16_pad(out, max_len, (uint16_t)(row + 1U), 2U, '0');
    str_append(out, max_len, " ");

    if (row >= g_box_count || g_box_buf[row].species == 0U) {
        str_append(out, max_len, "[Empty]");
        return;
    }

    const char *sp = bridge_species_name(g_box_buf[row].species);
    str_append(out, max_len, sp);
    str_append(out, max_len, " L");
    str_append_u32(out, max_len, (uint32_t)g_box_buf[row].level);
}

static void box_redraw_list(void)
{
    char row_buf[21];
    char line[21];
    for (uint8_t v = 0U; v < VIEWPORT_ROWS; v++) {
        uint8_t row = (uint8_t)(g_list.scroll_top + v);
        uint8_t y   = (uint8_t)(2U + v);
        if (row < g_list.count) {
            uint8_t selected = (row == g_list.selected);
            box_format_row(row, row_buf, sizeof(row_buf));
            str_clear(line);
            str_append(line, sizeof(line), selected ? "> " : "  ");
            str_append(line, sizeof(line), row_buf);
            display_draw_row(y, line, selected ? PAL_CURSOR : PAL_BODY);
        } else {
            display_draw_row(y, "", PAL_BODY);
        }
    }
}

void box_scene_init(void)
{
    int8_t prev_box  = bridge_pending_box_index();
    int8_t prev_slot = bridge_pending_box_slot();

    int cnt = bridge_ok() ? bridge_get_box_count() : 12;
    g_box_total = (cnt > 0) ? (uint8_t)cnt : 12U;

    if (prev_box >= 0 && (uint8_t)prev_box < g_box_total) {
        g_box_index = (uint8_t)prev_box;
    } else {
        g_box_index = 0U;
    }

    list_view_init(&g_list, BOX_SLOT_COUNT, VIEWPORT_ROWS);
    if (prev_slot >= 0 && (uint8_t)prev_slot < BOX_SLOT_COUNT) {
        g_list.selected = (uint8_t)prev_slot;
        if (g_list.selected >= VIEWPORT_ROWS) {
            g_list.scroll_top = (uint8_t)(g_list.selected - VIEWPORT_ROWS + 1U);
        }
    }
    bridge_clear_pending_box();
    box_load();

    display_clear(PAL_BODY);
    box_redraw_header();
    box_redraw_list();
    display_draw_footer(" A:Edit SEL:Box B:Bk");
}

scene_kind_t box_scene_update(void)
{
    if (input_pressed(J_B)) {
        bridge_clear_pending_box();
        return SCENE_MAIN_MENU;
    }

    if (input_pressed(J_SELECT)) {
        g_box_index       = (uint8_t)((g_box_index + 1U) % g_box_total);
        g_list.selected   = 0U;
        g_list.scroll_top = 0U;
        box_load();
        box_redraw_header();
        box_redraw_list();
        return SCENE_NONE;
    }

    if (list_view_tick(&g_list, 0U)) {
        box_redraw_list();
    }

    if (input_pressed(J_A)) {
        bridge_set_pending_box((int8_t)g_box_index, (int8_t)g_list.selected);
        return SCENE_POKEMON_EDIT;
    }

    return SCENE_NONE;
}
