/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <string.h>
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"

/* Tab indices */
#define TAB_IDENTITY 0U
#define TAB_STATS    1U
#define TAB_MOVES    2U

/* Edit modes */
#define MODE_NAV  0U
#define MODE_EDIT 1U

/* Identity tab rows (8 rows, matching frontend-gba) */
#define ROW_SPECIES  0U
#define ROW_NICKNAME 1U
#define ROW_OT_NAME  2U
#define ROW_ITEM     3U
#define ROW_ABILITY  4U
#define ROW_NATURE   5U
#define ROW_SHINY    6U
#define ROW_POKERUS  7U
#define IDENTITY_ROW_COUNT 8U

/* Stats tab rows (13 rows: Level + 6 EVs + 6 IVs) */
#define ROW_LEVEL  0U
#define ROW_EV_HP  1U
#define ROW_EV_ATK 2U
#define ROW_EV_DEF 3U
#define ROW_EV_SPA 4U
#define ROW_EV_SPD 5U
#define ROW_EV_SPE 6U
#define ROW_IV_HP  7U
#define ROW_IV_ATK 8U
#define ROW_IV_DEF 9U
#define ROW_IV_SPA 10U
#define ROW_IV_SPD 11U
#define ROW_IV_SPE 12U
#define STATS_ROW_COUNT 13U

/* Moves tab rows (8 rows: 4 × (Move, PP)) */
#define ROW_MOVE_0 0U
#define ROW_PP_0   1U
#define ROW_MOVE_1 2U
#define ROW_PP_1   3U
#define ROW_MOVE_2 4U
#define ROW_PP_2   5U
#define ROW_MOVE_3 6U
#define ROW_PP_3   7U
#define MOVES_ROW_COUNT 8U

#define VIEWPORT_ROWS 12U

/* Caller tags for text_input */
#define TAG_NICKNAME 1U
#define TAG_OT_NAME  2U

static const char * const k_ev_labels[6] = {
    "EV HP ", "EV Atk", "EV Def", "EV SpA", "EV SpD", "EV Spe"
};

static const char * const k_iv_labels[6] = {
    "DV HP ", "DV Atk", "DV Def", "DV SpA", "DV SpD", "DV Spe"
};

static pksave_pokemon_t g_mon;
static uint8_t          g_is_box = 0U;
static uint8_t          g_box    = 0U;
static uint8_t          g_slot   = 0U;
static uint8_t          g_tab    = TAB_IDENTITY;
static uint8_t          g_mode   = MODE_NAV;
static list_view_t      g_list;

static cycler_t         g_species;
static cycler_t         g_item;
static numeric_field_t  g_level;
static numeric_field_t  g_evs[6];
static numeric_field_t  g_ivs[6];
static cycler_t         g_moves[4];
static numeric_field_t  g_pps[4];

static uint8_t species_filter(uint16_t id)
{
    const char *name = bridge_species_name(id);
    if (name[0] == '?') return 0U;
    if (name[0] == 'M' && name[1] == 'i' && name[2] == 's') return 0U;
    return 1U;
}

static uint8_t item_filter(uint16_t id)
{
    if (id == 0U) return 1U;
    const char *name = bridge_item_name(id);
    return (name[0] != '?') ? 1U : 0U;
}

static uint8_t move_filter(uint16_t id)
{
    if (id == 0U) return 1U;
    const char *name = bridge_move_name(id);
    return (name[0] != '?') ? 1U : 0U;
}

static uint8_t max_pp_for_move(uint16_t move_id)
{
    uint8_t base = bridge_move_base_pp(move_id);
    return (uint8_t)(((uint16_t)base * 8U) / 5U);
}

static uint8_t row_count_for_tab(uint8_t tab)
{
    switch (tab) {
        case TAB_IDENTITY: return IDENTITY_ROW_COUNT;
        case TAB_STATS:    return STATS_ROW_COUNT;
        case TAB_MOVES:    return MOVES_ROW_COUNT;
        default:           return IDENTITY_ROW_COUNT;
    }
}

static void sync_hp_dv_and_shiny(void)
{
    uint8_t dv_atk = (uint8_t)((g_mon.ivs[1] / 2U) & 0x0FU);
    uint8_t dv_def = (uint8_t)((g_mon.ivs[2] / 2U) & 0x0FU);
    uint8_t dv_spd = (uint8_t)((g_mon.ivs[5] / 2U) & 0x0FU);
    uint8_t dv_spc = (uint8_t)((g_mon.ivs[3] / 2U) & 0x0FU);
    uint8_t dv_hp  = (uint8_t)(((dv_atk & 1U) << 3) | ((dv_def & 1U) << 2)
                             | ((dv_spd & 1U) << 1) |  (dv_spc & 1U));
    g_mon.ivs[0] = (uint8_t)(dv_hp * 2U);
    numeric_field_set_value(&g_ivs[0], (int32_t)dv_hp);
    g_mon.is_shiny = (dv_spd == 10U && dv_def == 10U && dv_spc == 10U &&
                      (dv_atk == 2U || dv_atk == 3U || dv_atk == 6U ||
                       dv_atk == 7U || dv_atk == 10U || dv_atk == 11U ||
                       dv_atk == 14U || dv_atk == 15U)) ? 1U : 0U;
}

static void load_from_save(void)
{
    uint8_t count = 0U;
    memset(&g_mon, 0, sizeof(g_mon));
    if (!bridge_ok()) return;

    if (g_is_box) {
        memset(g_box_buf, 0, sizeof(g_box_buf));
        bridge_get_box(g_box, g_box_buf, &count);
        if (g_slot < count) {
            g_mon = g_box_buf[g_slot];
        }
    } else {
        memset(g_party_buf, 0, sizeof(g_party_buf));
        bridge_get_party(g_party_buf, &count);
        if (g_slot < count) {
            g_mon = g_party_buf[g_slot];
        }
    }
    if (g_mon.species == 0U) {
        g_mon.species = 1U;
        g_mon.level   = 5U;
    }

    cycler_set_id(&g_species, g_mon.species);
    cycler_set_id(&g_item,    g_mon.held_item);
    numeric_field_set_value(&g_level, (int32_t)g_mon.level);

    for (uint8_t i = 0U; i < 6U; i++) {
        numeric_field_set_value(&g_evs[i], (int32_t)g_mon.evs[i]);
        numeric_field_set_value(&g_ivs[i], (int32_t)(g_mon.ivs[i] / 2U));
    }

    for (uint8_t m = 0U; m < 4U; m++) {
        cycler_set_id(&g_moves[m], g_mon.move_ids[m]);
        numeric_field_set_max(&g_pps[m], (int32_t)max_pp_for_move(g_mon.move_ids[m]));
        numeric_field_set_value(&g_pps[m], (int32_t)g_mon.move_pp[m]);
    }

    bridge_recalc_stats(&g_mon);
}

static void push_to_save(void)
{
    if (!bridge_ok()) return;
    g_mon.species   = g_species.id;
    g_mon.held_item = bridge_is_gen2() ? g_item.id : 0U;
    g_mon.level     = (uint8_t)g_level.value;

    for (uint8_t i = 0U; i < 6U; i++) {
        g_mon.evs[i] = (uint16_t)g_evs[i].value;
        g_mon.ivs[i] = (uint8_t)((uint8_t)g_ivs[i].value * 2U);
    }
    if (bridge_is_gen1()) {
        g_mon.evs[4] = g_mon.evs[3];
        g_mon.ivs[4] = g_mon.ivs[3];
    }
    sync_hp_dv_and_shiny();

    for (uint8_t m = 0U; m < 4U; m++) {
        g_mon.move_ids[m] = g_moves[m].id;
        g_mon.move_pp[m]  = (uint8_t)g_pps[m].value;
    }

    bridge_recalc_stats(&g_mon);

    if (g_is_box) {
        bridge_set_box_slot(g_box, g_slot, &g_mon);
    } else {
        bridge_set_party_slot(g_slot, &g_mon);
    }
    bridge_mark_dirty();
}

static void consume_text_input(void)
{
    if (!g_text_input.result_valid) return;
    g_text_input.result_valid = 0U;
    if (g_text_input.result_cancelled) return;

    if (g_text_input.result_tag == TAG_NICKNAME) {
        memset(g_mon.nickname, 0, sizeof(g_mon.nickname));
        for (uint8_t i = 0U;
             i < PKSAVE_GEN1_NICKNAME_MAX && g_text_input.result_value[i] != '\0';
             i++) {
            g_mon.nickname[i] = g_text_input.result_value[i];
        }
        push_to_save();
    } else if (g_text_input.result_tag == TAG_OT_NAME) {
        memset(g_mon.ot_name, 0, sizeof(g_mon.ot_name));
        for (uint8_t i = 0U;
             i < PKSAVE_GEN1_OT_NAME_MAX && g_text_input.result_value[i] != '\0';
             i++) {
            g_mon.ot_name[i] = g_text_input.result_value[i];
        }
        push_to_save();
    }
}

static void format_identity_row(uint8_t row, uint8_t editing, char *out, uint8_t max_len)
{
    str_clear(out);
    switch (row) {
        case ROW_SPECIES:
            str_append(out, max_len, editing ? "Spc :<" : "Spc : ");
            str_append(out, max_len, bridge_species_name(g_species.id));
            if (editing) str_append(out, max_len, ">");
            break;
        case ROW_NICKNAME:
            str_append(out, max_len, "Nick: ");
            str_append(out, max_len, g_mon.nickname);
            break;
        case ROW_OT_NAME:
            str_append(out, max_len, "OT  : ");
            str_append(out, max_len, g_mon.ot_name);
            break;
        case ROW_ITEM:
            str_append(out, max_len, editing ? "Item:<" : "Item: ");
            if (bridge_is_gen1()) {
                str_append(out, max_len, "N/A");
            } else if (g_item.id == 0U) {
                str_append(out, max_len, "(none)");
            } else {
                str_append(out, max_len, bridge_item_name(g_item.id));
            }
            if (editing) str_append(out, max_len, ">");
            break;
        case ROW_ABILITY:
            str_append(out, max_len, "Abil: N/A");
            break;
        case ROW_NATURE:
            str_append(out, max_len, "Natr: N/A");
            break;
        case ROW_SHINY:
            str_append(out, max_len, "Shny: ");
            str_append(out, max_len, g_mon.is_shiny ? "YES" : "no");
            break;
        case ROW_POKERUS:
            str_append(out, max_len, "Pkrs: ");
            if (bridge_is_gen1()) {
                str_append(out, max_len, "N/A");
            } else {
                pksave_pokerus_t pkrs;
                memset(&pkrs, 0, sizeof(pkrs));
                if (g_is_box) {
                    bridge_get_box_pokerus(g_box, g_slot, &pkrs);
                } else {
                    bridge_get_party_pokerus(g_slot, &pkrs);
                }
                if (pkrs.status == PKSAVE_PKRS_ACTIVE) {
                    str_append(out, max_len, "ACTIVE");
                } else if (pkrs.status == PKSAVE_PKRS_CURED) {
                    str_append(out, max_len, "CURED");
                } else {
                    str_append(out, max_len, "NEVER");
                }
            }
            break;
        default:
            break;
    }
}

static void format_stats_row(uint8_t row, uint8_t editing, char *out, uint8_t max_len)
{
    str_clear(out);
    if (row == ROW_LEVEL) {
        str_append(out, max_len, editing ? "Level : [" : "Level : ");
        str_append_u32(out, max_len, (uint32_t)g_level.value);
        if (editing) str_append(out, max_len, "]");
    } else if (row >= ROW_EV_HP && row <= ROW_EV_SPE) {
        uint8_t idx = (uint8_t)(row - ROW_EV_HP);
        str_append(out, max_len, k_ev_labels[idx]);
        str_append(out, max_len, editing ? ": [" : ": ");
        str_append_u32(out, max_len, (uint32_t)g_evs[idx].value);
        if (editing) str_append(out, max_len, "]");
    } else if (row >= ROW_IV_HP && row <= ROW_IV_SPE) {
        uint8_t idx = (uint8_t)(row - ROW_IV_HP);
        str_append(out, max_len, k_iv_labels[idx]);
        str_append(out, max_len, editing ? ": [" : ": ");
        str_append_u32(out, max_len, (uint32_t)g_ivs[idx].value);
        if (editing) str_append(out, max_len, "]");
    }
}

static void format_moves_row(uint8_t row, uint8_t editing, char *out, uint8_t max_len)
{
    uint8_t slot = (uint8_t)(row / 2U);
    uint8_t is_pp = (uint8_t)(row % 2U);
    str_clear(out);

    if (!is_pp) {
        str_append(out, max_len, "Mv");
        str_append_u32(out, max_len, (uint32_t)(slot + 1U));
        str_append(out, max_len, editing ? ":<" : ": ");
        if (g_moves[slot].id == 0U) {
            str_append(out, max_len, "(none)");
        } else {
            str_append(out, max_len, bridge_move_name(g_moves[slot].id));
        }
        if (editing) str_append(out, max_len, ">");
    } else {
        str_append(out, max_len, "PP");
        str_append_u32(out, max_len, (uint32_t)(slot + 1U));
        str_append(out, max_len, editing ? ": [" : ": ");
        str_append_u32(out, max_len, (uint32_t)g_pps[slot].value);
        str_append(out, max_len, "/");
        str_append_u32(out, max_len, (uint32_t)g_pps[slot].max_val);
        if (editing) str_append(out, max_len, "]");
    }
}

static void pokemon_edit_redraw(void)
{
    char row_buf[21];
    char line[21];

    /* Row 0: Header */
    str_clear(line);
    if (g_is_box) {
        str_append(line, sizeof(line), "BOX ");
        str_append_u32(line, sizeof(line), (uint32_t)(g_box + 1U));
        str_append(line, sizeof(line), " SLOT ");
        str_append_u32(line, sizeof(line), (uint32_t)(g_slot + 1U));
    } else {
        str_append(line, sizeof(line), "PARTY SLOT ");
        str_append_u32(line, sizeof(line), (uint32_t)(g_slot + 1U));
    }
    display_draw_header(line, bridge_is_dirty());

    /* Row 1: Tab bar */
    if (g_tab == TAB_IDENTITY) {
        display_draw_subheader("[IDENT] STATS MOVES");
    } else if (g_tab == TAB_STATS) {
        display_draw_subheader(" IDENT [STATS] MOVES");
    } else {
        display_draw_subheader(" IDENT STATS [MOVES]");
    }

    /* Row 2: Pokemon summary anchor */
    str_clear(line);
    str_append(line, sizeof(line), " \x7F ");
    str_append(line, sizeof(line), bridge_species_name(g_species.id));
    str_append(line, sizeof(line), " Lv");
    str_append_u32(line, sizeof(line), (uint32_t)g_level.value);
    display_draw_row(2U, line, PAL_SUBHDR);

    /* Rows 3..14: List viewport */
    for (uint8_t v = 0U; v < VIEWPORT_ROWS; v++) {
        uint8_t row = (uint8_t)(g_list.scroll_top + v);
        uint8_t y   = (uint8_t)(3U + v);
        if (row < g_list.count) {
            uint8_t selected = (row == g_list.selected);
            uint8_t editing  = (selected && g_mode == MODE_EDIT);
            if (g_tab == TAB_IDENTITY) {
                format_identity_row(row, editing, row_buf, sizeof(row_buf));
            } else if (g_tab == TAB_STATS) {
                format_stats_row(row, editing, row_buf, sizeof(row_buf));
            } else {
                format_moves_row(row, editing, row_buf, sizeof(row_buf));
            }
            str_clear(line);
            str_append(line, sizeof(line), selected ? "> " : "  ");
            str_append(line, sizeof(line), row_buf);
            display_draw_row(y, line, editing ? PAL_EDIT : (selected ? PAL_CURSOR : PAL_BODY));
        } else {
            display_draw_row(y, "", PAL_BODY);
        }
    }

    /* Rows 15..16: Live calculated stats summary */
    str_clear(line);
    str_append(line, sizeof(line), "HP:");
    str_append_u32(line, sizeof(line), (uint32_t)g_mon.stat[0]);
    str_append(line, sizeof(line), " A:");
    str_append_u32(line, sizeof(line), (uint32_t)g_mon.stat[1]);
    str_append(line, sizeof(line), " D:");
    str_append_u32(line, sizeof(line), (uint32_t)g_mon.stat[2]);
    display_draw_row(15U, line, PAL_SUBHDR);

    str_clear(line);
    str_append(line, sizeof(line), "SA:");
    str_append_u32(line, sizeof(line), (uint32_t)g_mon.stat[3]);
    str_append(line, sizeof(line), " SD:");
    str_append_u32(line, sizeof(line), (uint32_t)g_mon.stat[4]);
    str_append(line, sizeof(line), " Sp:");
    str_append_u32(line, sizeof(line), (uint32_t)g_mon.stat[5]);
    display_draw_row(16U, line, PAL_SUBHDR);

    /* Row 17: Footer */
    if (g_mode == MODE_EDIT) {
        display_draw_footer(" DPad:Adj A/B:Done");
    } else {
        display_draw_footer(" A:Edit L/R:Tab B:Bk");
    }
}

static void arm_focused_widget(void)
{
    uint8_t sel = g_list.selected;
    if (g_tab == TAB_IDENTITY) {
        if (sel == ROW_SPECIES) cycler_on_focus(&g_species);
        if (sel == ROW_ITEM)    cycler_on_focus(&g_item);
    } else if (g_tab == TAB_STATS) {
        if (sel == ROW_LEVEL) {
            numeric_field_on_focus(&g_level);
        } else if (sel >= ROW_EV_HP && sel <= ROW_EV_SPE) {
            numeric_field_on_focus(&g_evs[sel - ROW_EV_HP]);
        } else if (sel >= ROW_IV_HP && sel <= ROW_IV_SPE) {
            numeric_field_on_focus(&g_ivs[sel - ROW_IV_HP]);
        }
    } else if (g_tab == TAB_MOVES) {
        uint8_t slot = (uint8_t)(sel / 2U);
        if ((sel % 2U) == 0U) {
            cycler_on_focus(&g_moves[slot]);
        } else {
            numeric_field_on_focus(&g_pps[slot]);
        }
    }
}

void pokemon_edit_scene_init(void)
{
    if (bridge_pending_box_index() >= 0) {
        g_is_box = 1U;
        g_box    = (uint8_t)bridge_pending_box_index();
        g_slot   = (uint8_t)bridge_pending_box_slot();
        if (g_slot >= 20U) g_slot = 0U;
    } else {
        g_is_box = 0U;
        g_box    = 0U;
        int8_t s = bridge_pending_party_slot();
        g_slot   = (s >= 0 && s < 6) ? (uint8_t)s : 0U;
    }

    uint16_t sp_max   = bridge_is_gen1() ? 190U : 251U;
    uint16_t move_max = bridge_is_gen1() ? 165U : 251U;

    cycler_init(&g_species, 1U, 1U, sp_max, species_filter);
    cycler_init(&g_item,    0U, 0U, 255U,   item_filter);
    numeric_field_init(&g_level, 5L, 1L, 100L);

    for (uint8_t i = 0U; i < 6U; i++) {
        numeric_field_init(&g_evs[i], 0L, 0L, 65535L);
        numeric_field_init(&g_ivs[i], 0L, 0L, 15L);
    }
    for (uint8_t m = 0U; m < 4U; m++) {
        cycler_init(&g_moves[m], 0U, 0U, move_max, move_filter);
        numeric_field_init(&g_pps[m], 0L, 0L, 0L);
    }

    g_tab  = TAB_IDENTITY;
    g_mode = MODE_NAV;
    list_view_init(&g_list, row_count_for_tab(g_tab), VIEWPORT_ROWS);

    load_from_save();
    consume_text_input();
    arm_focused_widget();

    display_clear(PAL_BODY);
    pokemon_edit_redraw();
}

static uint8_t tick_edit_widget(void)
{
    uint8_t sel = g_list.selected;
    if (g_tab == TAB_IDENTITY) {
        if (sel == ROW_SPECIES) return cycler_tick(&g_species);
        if (sel == ROW_ITEM && bridge_is_gen2()) return cycler_tick(&g_item);
    } else if (g_tab == TAB_STATS) {
        if (sel == ROW_LEVEL) return numeric_field_tick(&g_level);
        if (sel >= ROW_EV_HP && sel <= ROW_EV_SPE) {
            uint8_t idx = (uint8_t)(sel - ROW_EV_HP);
            if (numeric_field_tick(&g_evs[idx])) {
                if (bridge_is_gen1() && (idx == 3U || idx == 4U)) {
                    numeric_field_set_value(&g_evs[3], g_evs[idx].value);
                    numeric_field_set_value(&g_evs[4], g_evs[idx].value);
                }
                return 1U;
            }
        }
        if (sel >= ROW_IV_ATK && sel <= ROW_IV_SPE) {
            uint8_t idx = (uint8_t)(sel - ROW_IV_HP);
            if (numeric_field_tick(&g_ivs[idx])) {
                if (bridge_is_gen1() && (idx == 3U || idx == 4U)) {
                    numeric_field_set_value(&g_ivs[3], g_ivs[idx].value);
                    numeric_field_set_value(&g_ivs[4], g_ivs[idx].value);
                }
                return 1U;
            }
        }
    } else if (g_tab == TAB_MOVES) {
        uint8_t slot = (uint8_t)(sel / 2U);
        if ((sel % 2U) == 0U) {
            if (cycler_tick(&g_moves[slot])) {
                uint8_t base_pp = bridge_move_base_pp(g_moves[slot].id);
                uint8_t max_pp  = max_pp_for_move(g_moves[slot].id);
                numeric_field_set_max(&g_pps[slot], (int32_t)max_pp);
                numeric_field_set_value(&g_pps[slot], (int32_t)base_pp);
                return 1U;
            }
        } else {
            return numeric_field_tick(&g_pps[slot]);
        }
    }
    return 0U;
}

static uint8_t can_enter_edit_mode(uint8_t tab, uint8_t sel)
{
    if (tab == TAB_IDENTITY) {
        if (sel == ROW_SPECIES) return 1U;
        if (sel == ROW_ITEM && bridge_is_gen2()) return 1U;
        return 0U;
    }
    if (tab == TAB_STATS) {
        if (sel == ROW_IV_HP) return 0U; /* HP DV is derived */
        return 1U;
    }
    if (tab == TAB_MOVES) {
        return 1U;
    }
    return 0U;
}

scene_kind_t pokemon_edit_scene_update(void)
{
    if (g_mode == MODE_EDIT) {
        if (input_pressed(J_A) || input_pressed(J_B)) {
            g_mode = MODE_NAV;
            pokemon_edit_redraw();
            return SCENE_NONE;
        }
        if (tick_edit_widget()) {
            push_to_save();
            pokemon_edit_redraw();
        }
        return SCENE_NONE;
    }

    /* MODE_NAV */
    if (input_pressed(J_B)) {
        return g_is_box ? SCENE_BOXES : SCENE_PARTY;
    }

    if (input_pressed(J_LEFT) && g_tab > 0U) {
        g_tab--;
        list_view_init(&g_list, row_count_for_tab(g_tab), VIEWPORT_ROWS);
        arm_focused_widget();
        pokemon_edit_redraw();
        return SCENE_NONE;
    }
    if (input_pressed(J_RIGHT) && g_tab < 2U) {
        g_tab++;
        list_view_init(&g_list, row_count_for_tab(g_tab), VIEWPORT_ROWS);
        arm_focused_widget();
        pokemon_edit_redraw();
        return SCENE_NONE;
    }

    if (list_view_tick(&g_list, 0U)) {
        arm_focused_widget();
        pokemon_edit_redraw();
        return SCENE_NONE;
    }

    if (input_pressed(J_A)) {
        uint8_t sel = g_list.selected;
        if (g_tab == TAB_IDENTITY && sel == ROW_NICKNAME) {
            memset(&g_text_input, 0, sizeof(g_text_input));
            for (uint8_t i = 0U;
                 i < PKSAVE_GEN1_NICKNAME_MAX && g_mon.nickname[i] != '\0';
                 i++) {
                g_text_input.initial[i] = g_mon.nickname[i];
            }
            g_text_input.max_len      = PKSAVE_GEN1_NICKNAME_MAX;
            g_text_input.return_to    = SCENE_POKEMON_EDIT;
            g_text_input.caller_tag   = TAG_NICKNAME;
            g_text_input.result_valid = 0U;
            return SCENE_TEXT_INPUT;
        }
        if (g_tab == TAB_IDENTITY && sel == ROW_OT_NAME) {
            memset(&g_text_input, 0, sizeof(g_text_input));
            for (uint8_t i = 0U;
                 i < PKSAVE_GEN1_OT_NAME_MAX && g_mon.ot_name[i] != '\0';
                 i++) {
                g_text_input.initial[i] = g_mon.ot_name[i];
            }
            g_text_input.max_len      = PKSAVE_GEN1_OT_NAME_MAX;
            g_text_input.return_to    = SCENE_POKEMON_EDIT;
            g_text_input.caller_tag   = TAG_OT_NAME;
            g_text_input.result_valid = 0U;
            return SCENE_TEXT_INPUT;
        }
        if (g_tab == TAB_IDENTITY && sel == ROW_SHINY) {
            /* Toggle shiny DVs (Atk=10,Def=10,Spd=10,Spc=10 <-> Atk=15) */
            if (g_mon.is_shiny) {
                numeric_field_set_value(&g_ivs[1], 15L);
            } else {
                numeric_field_set_value(&g_ivs[1], 10L);
                numeric_field_set_value(&g_ivs[2], 10L);
                numeric_field_set_value(&g_ivs[3], 10L);
                numeric_field_set_value(&g_ivs[4], 10L);
                numeric_field_set_value(&g_ivs[5], 10L);
            }
            push_to_save();
            pokemon_edit_redraw();
            return SCENE_NONE;
        }
        if (g_tab == TAB_IDENTITY && sel == ROW_POKERUS && bridge_is_gen2()) {
            return SCENE_POKERUS_MODAL;
        }
        if (can_enter_edit_mode(g_tab, sel)) {
            g_mode = MODE_EDIT;
            arm_focused_widget();
            pokemon_edit_redraw();
            return SCENE_NONE;
        }
    }

    return SCENE_NONE;
}
