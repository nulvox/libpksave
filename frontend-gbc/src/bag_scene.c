/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <string.h>
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"

#define VIEWPORT_ROWS 14U

static const pksave_pocket_t k_gen2_pockets[4] = {
    PKSAVE_POCKET_ITEMS,
    PKSAVE_POCKET_BALLS,
    PKSAVE_POCKET_KEY,
    PKSAVE_POCKET_TM_HM,
};

static const char * const k_pocket_labels[4] = {
    "ITEMS",
    "BALLS",
    "KEY",
    "TM/HM",
};

static const uint8_t k_gen2_pocket_caps[4] = { 20U, 12U, 25U, 57U };

static uint8_t     g_pocket_idx = 0U;
static uint8_t     g_item_count = 0U;
static list_view_t g_list;

static pksave_pocket_t current_pocket(void)
{
    if (bridge_is_gen1()) return PKSAVE_POCKET_ITEMS;
    return k_gen2_pockets[g_pocket_idx];
}

static uint8_t current_pocket_cap(void)
{
    if (bridge_is_gen1()) return 20U;
    return k_gen2_pocket_caps[g_pocket_idx];
}

static void bag_load_pocket(void)
{
    memset(g_bag_buf, 0, sizeof(g_bag_buf));
    g_item_count = 0U;
    if (bridge_ok()) {
        bridge_get_pocket(current_pocket(), g_bag_buf, &g_item_count);
    }
    list_view_set_count(&g_list, current_pocket_cap());
}

static void bag_redraw_header(void)
{
    char title[20];
    str_clear(title);
    str_append(title, sizeof(title), "BAG ");
    str_append(title, sizeof(title), bridge_is_gen1() ? "ITEMS" : k_pocket_labels[g_pocket_idx]);
    display_draw_header(title, bridge_is_dirty());

    if (bridge_is_gen2()) {
        display_draw_subheader(" < L/R: Pocket >");
    } else {
        display_draw_subheader(" ITEMS POCKET");
    }
}

static void bag_format_row(uint8_t row, char *out, uint8_t max_len)
{
    str_clear(out);
    if (row >= g_item_count || g_bag_buf[row].item_id == 0U) {
        str_append(out, max_len, "[Empty]");
        return;
    }
    const char *name = bridge_item_name(g_bag_buf[row].item_id);
    str_append(out, max_len, name);
    str_append(out, max_len, " x");
    str_append_u32(out, max_len, (uint32_t)g_bag_buf[row].quantity);
}

static void bag_redraw_list(void)
{
    char row_buf[21];
    char line[21];
    for (uint8_t v = 0U; v < VIEWPORT_ROWS; v++) {
        uint8_t row = (uint8_t)(g_list.scroll_top + v);
        uint8_t y   = (uint8_t)(2U + v);
        if (row < g_list.count) {
            uint8_t selected = (row == g_list.selected);
            bag_format_row(row, row_buf, sizeof(row_buf));
            str_clear(line);
            str_append(line, sizeof(line), selected ? "> " : "  ");
            str_append(line, sizeof(line), row_buf);
            display_draw_row(y, line, selected ? PAL_CURSOR : PAL_BODY);
        } else {
            display_draw_row(y, "", PAL_BODY);
        }
    }
}

void bag_scene_init(void)
{
    pksave_pocket_t pending_p = bridge_pending_bag_pocket();
    int8_t          pending_s = bridge_pending_bag_slot();

    g_pocket_idx = 0U;
    if (bridge_is_gen2() && pending_p != PKSAVE_POCKET_NONE) {
        for (uint8_t i = 0U; i < 4U; i++) {
            if (k_gen2_pockets[i] == pending_p) {
                g_pocket_idx = i;
                break;
            }
        }
    }

    list_view_init(&g_list, current_pocket_cap(), VIEWPORT_ROWS);
    bag_load_pocket();

    if (pending_s >= 0 && (uint8_t)pending_s < g_list.count) {
        g_list.selected = (uint8_t)pending_s;
        if (g_list.selected >= VIEWPORT_ROWS) {
            g_list.scroll_top = (uint8_t)(g_list.selected - VIEWPORT_ROWS + 1U);
        }
    }
    bridge_clear_pending_bag();

    display_clear(PAL_BODY);
    bag_redraw_header();
    bag_redraw_list();
    display_draw_footer(" A:Edit L/R:Pkt B:Bk");
}

scene_kind_t bag_scene_update(void)
{
    if (input_pressed(J_B)) {
        return SCENE_MAIN_MENU;
    }

    if (bridge_is_gen2()) {
        if (input_pressed(J_LEFT) && g_pocket_idx > 0U) {
            g_pocket_idx--;
            g_list.selected   = 0U;
            g_list.scroll_top = 0U;
            bag_load_pocket();
            bag_redraw_header();
            bag_redraw_list();
            return SCENE_NONE;
        }
        if (input_pressed(J_RIGHT) && g_pocket_idx < 3U) {
            g_pocket_idx++;
            g_list.selected   = 0U;
            g_list.scroll_top = 0U;
            bag_load_pocket();
            bag_redraw_header();
            bag_redraw_list();
            return SCENE_NONE;
        }
    }

    if (list_view_tick(&g_list, 0U)) {
        bag_redraw_list();
    }

    if (input_pressed(J_A)) {
        bridge_set_pending_bag(current_pocket(), (int8_t)g_list.selected);
        return SCENE_SLOT_EDIT;
    }

    return SCENE_NONE;
}
