/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <string.h>
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"

#define ROW_ITEM  0U
#define ROW_QTY   1U
#define ROW_COUNT 2U

#define ITEM_MIN 0U
#define ITEM_MAX 255U

static pksave_pocket_t g_pocket = PKSAVE_POCKET_ITEMS;
static int8_t          g_slot   = 0;
static list_view_t     g_list;
static cycler_t        g_item;
static numeric_field_t g_qty;

static uint8_t pocket_filter(uint16_t id)
{
    if (id == 0U) return 1U;
    return bridge_item_pocket(id) == g_pocket ? 1U : 0U;
}

static void resync_qty_max(void)
{
    uint16_t cap = bridge_item_max_stack(g_item.id);
    numeric_field_set_max(&g_qty, (int32_t)cap);
}

static void slot_edit_redraw(void)
{
    char line[21];

    str_clear(line);
    str_append(line, sizeof(line), "EDIT SLOT ");
    str_append_u32(line, sizeof(line), (uint32_t)g_slot);
    display_draw_header(line, bridge_is_dirty());
    display_draw_subheader(" ITEM & QUANTITY");
    display_clear_rows(2U, 16U, PAL_BODY);

    /* Row 0: Item */
    const char *name = (g_item.id == 0U) ? "(none)" : bridge_item_name(g_item.id);
    str_clear(line);
    str_append(line, sizeof(line), (g_list.selected == ROW_ITEM) ? "> Item:<" : "  Item:<");
    str_append(line, sizeof(line), name);
    str_append(line, sizeof(line), ">");
    display_draw_row(4U, line, (g_list.selected == ROW_ITEM) ? PAL_CURSOR : PAL_BODY);

    /* Row 1: Qty */
    str_clear(line);
    str_append(line, sizeof(line), (g_list.selected == ROW_QTY) ? "> Qty : " : "  Qty : ");
    str_append_u32(line, sizeof(line), (uint32_t)g_qty.value);
    str_append(line, sizeof(line), " / ");
    str_append_u32(line, sizeof(line), (uint32_t)g_qty.max_val);
    display_draw_row(6U, line, (g_list.selected == ROW_QTY) ? PAL_CURSOR : PAL_BODY);

    display_draw_footer(" L/R:Edit A:OK B:Back");
}

void slot_edit_scene_init(void)
{
    uint8_t count = 0U;
    g_pocket = bridge_pending_bag_pocket();
    g_slot   = bridge_pending_bag_slot();
    if (g_slot < 0) g_slot = 0;

    list_view_init(&g_list, ROW_COUNT, ROW_COUNT);
    cycler_init(&g_item, ITEM_MIN, ITEM_MIN, ITEM_MAX, pocket_filter);
    numeric_field_init(&g_qty, 0L, 0L, 99L);

    memset(g_bag_buf, 0, sizeof(g_bag_buf));
    if (bridge_ok()) {
        bridge_get_pocket(g_pocket, g_bag_buf, &count);
    }
    if ((uint8_t)g_slot < count) {
        cycler_set_id(&g_item, g_bag_buf[g_slot].item_id);
        numeric_field_set_value(&g_qty, (int32_t)g_bag_buf[g_slot].quantity);
    } else {
        cycler_set_id(&g_item, ITEM_MIN);
        numeric_field_set_value(&g_qty, 0L);
    }
    resync_qty_max();
    cycler_on_focus(&g_item);

    display_clear(PAL_BODY);
    slot_edit_redraw();
}

scene_kind_t slot_edit_scene_update(void)
{
    if (input_pressed(J_B)) {
        return SCENE_BAG;
    }

    uint8_t sel_changed = list_view_tick(&g_list, 0U);
    uint8_t sel         = g_list.selected;
    uint8_t need_redraw = sel_changed;

    if (sel_changed) {
        if (sel == ROW_ITEM) cycler_on_focus(&g_item);
        if (sel == ROW_QTY)  numeric_field_on_focus(&g_qty);
    }

    if (sel == ROW_ITEM) {
        if (cycler_tick(&g_item)) {
            resync_qty_max();
            need_redraw = 1U;
        }
    } else {
        if (numeric_field_tick(&g_qty)) {
            need_redraw = 1U;
        }
    }

    if (need_redraw) {
        slot_edit_redraw();
    }

    if (input_pressed(J_A)) {
        pksave_item_t out;
        out.item_id  = g_item.id;
        out.quantity = (out.item_id == 0U) ? 0U : (uint16_t)g_qty.value;

        if (bridge_ok()) {
            if (bridge_set_item(g_pocket, (uint8_t)g_slot, &out) == PKSAVE_OK) {
                bridge_mark_dirty();
            }
        }
        return SCENE_BAG;
    }

    return SCENE_NONE;
}
