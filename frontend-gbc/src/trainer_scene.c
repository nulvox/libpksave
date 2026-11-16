/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <string.h>
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"

#define ROW_NAME       0U
#define ROW_TID        1U
#define ROW_MONEY      2U
#define ROW_BADGE_0    3U
#define VIEWPORT_ROWS  14U
#define MONEY_MAX      999999L

static pksave_trainer_t g_trainer;
static list_view_t      g_list;
static numeric_field_t  g_tid;
static numeric_field_t  g_money;
static uint8_t          g_badge_count = 8U;

static void trainer_load_from_save(void)
{
    if (!bridge_ok()) return;
    bridge_get_trainer(&g_trainer);
    numeric_field_set_value(&g_tid,   (int32_t)g_trainer.tid);
    numeric_field_set_value(&g_money, (int32_t)g_trainer.money);
}

static void trainer_push_to_save(void)
{
    if (!bridge_ok()) return;
    g_trainer.tid   = (uint16_t)g_tid.value;
    g_trainer.money = (uint32_t)g_money.value;
    bridge_set_trainer(&g_trainer);
    bridge_mark_dirty();
}

static void trainer_consume_text_input(void)
{
    if (!g_text_input.result_valid) return;
    g_text_input.result_valid = 0U;
    if (g_text_input.result_cancelled) return;

    memset(g_trainer.name, 0, sizeof(g_trainer.name));
    for (uint8_t i = 0U;
         i < PKSAVE_GEN1_TRAINER_NAME_MAX && g_text_input.result_value[i] != '\0';
         i++) {
        g_trainer.name[i] = g_text_input.result_value[i];
    }
    trainer_push_to_save();
}

static void trainer_format_row(uint8_t row, char *out, uint8_t max_len)
{
    str_clear(out);
    if (row == ROW_NAME) {
        str_append(out, max_len, "Name : ");
        str_append(out, max_len, g_trainer.name);
    } else if (row == ROW_TID) {
        str_append(out, max_len, "TID  : ");
        str_append_u32(out, max_len, (uint32_t)g_tid.value);
    } else if (row == ROW_MONEY) {
        str_append(out, max_len, "Money: ");
        str_append_u32(out, max_len, (uint32_t)g_money.value);
    } else {
        uint8_t b = (uint8_t)(row - ROW_BADGE_0);
        uint8_t on = (uint8_t)((g_trainer.badges >> b) & 1U);
        str_append(out, max_len, "Badge ");
        str_append_u32(out, max_len, (uint32_t)(b + 1U));
        str_append(out, max_len, (b + 1U < 10U) ? " : " : ": ");
        str_append(out, max_len, on ? "ON" : "off");
    }
}

static void trainer_redraw_list(void)
{
    char row_buf[21];
    char line[21];
    for (uint8_t v = 0U; v < VIEWPORT_ROWS; v++) {
        uint8_t row = (uint8_t)(g_list.scroll_top + v);
        uint8_t y   = (uint8_t)(2U + v);
        if (row < g_list.count) {
            uint8_t selected = (row == g_list.selected);
            trainer_format_row(row, row_buf, sizeof(row_buf));
            str_clear(line);
            str_append(line, sizeof(line), selected ? "> " : "  ");
            str_append(line, sizeof(line), row_buf);
            display_draw_row(y, line, selected ? PAL_CURSOR : PAL_BODY);
        } else {
            display_draw_row(y, "", PAL_BODY);
        }
    }
}

static void trainer_redraw_header(void)
{
    display_draw_header("TRAINER", bridge_is_dirty());
}

void trainer_scene_init(void)
{
    g_badge_count = bridge_is_gen2() ? 16U : 8U;
    list_view_init(&g_list, (uint8_t)(ROW_BADGE_0 + g_badge_count), VIEWPORT_ROWS);
    numeric_field_init(&g_tid,   0L, 0L, 65535L);
    numeric_field_init(&g_money, 0L, 0L, MONEY_MAX);

    trainer_load_from_save();
    trainer_consume_text_input();

    display_clear(PAL_BODY);
    trainer_redraw_header();
    display_draw_subheader(" PROFILE");
    trainer_redraw_list();
    display_draw_footer(" A:Edit L/R:Step B:Bk");
}

scene_kind_t trainer_scene_update(void)
{
    uint8_t need_list   = 0U;
    uint8_t need_header = 0U;

    if (input_pressed(J_B)) {
        return SCENE_MAIN_MENU;
    }

    uint8_t sel_changed = list_view_tick(&g_list, 0U);
    uint8_t sel         = g_list.selected;

    if (sel_changed) {
        if (sel == ROW_TID)   numeric_field_on_focus(&g_tid);
        if (sel == ROW_MONEY) numeric_field_on_focus(&g_money);
        need_list = 1U;
    }

    if (sel == ROW_NAME) {
        if (input_pressed(J_A)) {
            memset(&g_text_input, 0, sizeof(g_text_input));
            for (uint8_t i = 0U;
                 i < PKSAVE_GEN1_TRAINER_NAME_MAX && g_trainer.name[i] != '\0';
                 i++) {
                g_text_input.initial[i] = g_trainer.name[i];
            }
            g_text_input.max_len      = PKSAVE_GEN1_TRAINER_NAME_MAX;
            g_text_input.return_to    = SCENE_TRAINER;
            g_text_input.caller_tag   = 0U;
            g_text_input.result_valid = 0U;
            return SCENE_TEXT_INPUT;
        }
    } else if (sel == ROW_TID) {
        if (numeric_field_tick(&g_tid)) {
            trainer_push_to_save();
            need_list   = 1U;
            need_header = 1U;
        }
    } else if (sel == ROW_MONEY) {
        if (numeric_field_tick(&g_money)) {
            trainer_push_to_save();
            need_list   = 1U;
            need_header = 1U;
        }
    } else {
        if (input_pressed(J_A)) {
            uint8_t b = (uint8_t)(sel - ROW_BADGE_0);
            g_trainer.badges ^= (uint16_t)(1U << b);
            trainer_push_to_save();
            need_list   = 1U;
            need_header = 1U;
        }
    }

    if (need_list)   trainer_redraw_list();
    if (need_header) trainer_redraw_header();

    return SCENE_NONE;
}
