/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"

#define MENU_ENTRY_COUNT 5U

static const char * const k_menu_labels[MENU_ENTRY_COUNT] = {
    "Trainer",
    "Party",
    "Bag",
    "Boxes",
    "Save & Exit",
};

static const scene_kind_t k_menu_targets[MENU_ENTRY_COUNT] = {
    SCENE_TRAINER,
    SCENE_PARTY,
    SCENE_BAG,
    SCENE_BOXES,
    SCENE_SAVE,
};

static uint8_t g_menu_cursor = 0U;
static uint8_t g_was_dirty   = 0U;

static void main_menu_redraw(void)
{
    char line[21];
    display_draw_header(bridge_game_display_name(), bridge_is_dirty());
    display_draw_subheader(" MAIN MENU");
    display_clear_rows(2U, 16U, PAL_BODY);

    for (uint8_t i = 0U; i < MENU_ENTRY_COUNT; i++) {
        uint8_t y = (uint8_t)(4U + i * 2U);
        uint8_t selected = (i == g_menu_cursor);
        str_clear(line);
        str_append(line, sizeof(line), selected ? " > \x7F " : "   \x7F ");
        str_append(line, sizeof(line), k_menu_labels[i]);
        display_draw_row(y, line, selected ? PAL_CURSOR : PAL_BODY);
    }

    display_draw_footer(" U/D:Move  A:Select");
}

void main_menu_scene_init(void)
{
    g_menu_cursor = 0U;
    g_was_dirty   = bridge_is_dirty();
    display_clear(PAL_BODY);
    main_menu_redraw();
}

scene_kind_t main_menu_scene_update(void)
{
    uint8_t need_redraw = 0U;

    if (input_pressed(J_UP)) {
        g_menu_cursor = (uint8_t)((g_menu_cursor + MENU_ENTRY_COUNT - 1U) % MENU_ENTRY_COUNT);
        need_redraw = 1U;
    } else if (input_pressed(J_DOWN)) {
        g_menu_cursor = (uint8_t)((g_menu_cursor + 1U) % MENU_ENTRY_COUNT);
        need_redraw = 1U;
    }

    if (input_pressed(J_A)) {
        return k_menu_targets[g_menu_cursor];
    }

    if (bridge_is_dirty() != g_was_dirty) {
        g_was_dirty = bridge_is_dirty();
        need_redraw = 1U;
    }

    if (need_redraw) {
        main_menu_redraw();
    }

    return SCENE_NONE;
}
