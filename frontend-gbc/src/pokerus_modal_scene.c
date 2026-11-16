/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <string.h>
#include "scenes.h"
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"

#define ROW_STATUS 0U
#define ROW_STRAIN 1U
#define ROW_DAYS   2U

#define STRAIN_MIN 1U
#define STRAIN_MAX 15U
#define DAYS_MIN   0U
#define DAYS_MAX   15U

static uint8_t                 g_is_box = 0U;
static uint8_t                 g_box    = 0U;
static uint8_t                 g_slot   = 0U;
static pksave_pokerus_status_t g_status = PKSAVE_PKRS_NEVER;
static uint8_t                 g_strain = 0U;
static uint8_t                 g_days   = 0U;
static uint8_t                 g_cursor = ROW_STATUS;

static uint8_t visible_rows(void)
{
    switch (g_status) {
        case PKSAVE_PKRS_NEVER:  return 1U;
        case PKSAVE_PKRS_ACTIVE: return 3U;
        case PKSAVE_PKRS_CURED:  return 2U;
        default:                 return 1U;
    }
}

static void cycle_status(int8_t dir)
{
    int8_t next = (int8_t)((int8_t)g_status + dir);
    next = (int8_t)(((next % 3) + 3) % 3);
    pksave_pokerus_status_t ns = (pksave_pokerus_status_t)next;
    if (ns == g_status) return;

    switch (ns) {
        case PKSAVE_PKRS_NEVER:
            g_days = 0U;
            break;
        case PKSAVE_PKRS_ACTIVE:
            if (g_strain == 0U) g_strain = 1U;
            if (g_days   == 0U) g_days   = 1U;
            break;
        case PKSAVE_PKRS_CURED:
            if (g_strain == 0U) g_strain = 1U;
            g_days = 0U;
            break;
        default:
            break;
    }
    g_status = ns;
    if (g_cursor >= visible_rows()) {
        g_cursor = (uint8_t)(visible_rows() - 1U);
    }
}

static void step_strain(int8_t delta)
{
    int8_t v = (int8_t)((int8_t)g_strain + delta);
    if (v < (int8_t)STRAIN_MIN) v = (int8_t)STRAIN_MIN;
    if (v > (int8_t)STRAIN_MAX) v = (int8_t)STRAIN_MAX;
    g_strain = (uint8_t)v;
}

static void step_days(int8_t delta)
{
    int8_t v = (int8_t)((int8_t)g_days + delta);
    if (v < (int8_t)DAYS_MIN) v = (int8_t)DAYS_MIN;
    if (v > (int8_t)DAYS_MAX) v = (int8_t)DAYS_MAX;
    g_days = (uint8_t)v;
}

static void pokerus_redraw(void)
{
    char line[21];

    display_draw_header("POKERUS", bridge_is_dirty());
    display_draw_subheader(" STATUS & STRAIN");
    display_clear_rows(2U, 16U, PAL_BODY);

    /* Row 0: Status */
    str_clear(line);
    str_append(line, sizeof(line), (g_cursor == ROW_STATUS) ? "> Status:<" : "  Status:<");
    if (g_status == PKSAVE_PKRS_ACTIVE) {
        str_append(line, sizeof(line), "ACTIVE>");
    } else if (g_status == PKSAVE_PKRS_CURED) {
        str_append(line, sizeof(line), "CURED>");
    } else {
        str_append(line, sizeof(line), "NEVER>");
    }
    display_draw_row(4U, line, (g_cursor == ROW_STATUS) ? PAL_CURSOR : PAL_BODY);

    /* Row 1: Strain (visible for ACTIVE and CURED) */
    if (visible_rows() >= 2U) {
        str_clear(line);
        str_append(line, sizeof(line), (g_cursor == ROW_STRAIN) ? "> Strain: " : "  Strain: ");
        str_append_u32(line, sizeof(line), (uint32_t)g_strain);
        display_draw_row(6U, line, (g_cursor == ROW_STRAIN) ? PAL_CURSOR : PAL_BODY);
    }

    /* Row 2: Days (visible for ACTIVE) */
    if (visible_rows() >= 3U) {
        str_clear(line);
        str_append(line, sizeof(line), (g_cursor == ROW_DAYS) ? "> Days  : " : "  Days  : ");
        str_append_u32(line, sizeof(line), (uint32_t)g_days);
        display_draw_row(8U, line, (g_cursor == ROW_DAYS) ? PAL_CURSOR : PAL_BODY);
    }

    display_draw_footer(" L/R:Set A:OK B:Back");
}

void pokerus_modal_scene_init(void)
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

    g_status = PKSAVE_PKRS_NEVER;
    g_strain = 0U;
    g_days   = 0U;
    g_cursor = ROW_STATUS;

    if (bridge_ok()) {
        pksave_pokerus_t pkrs;
        memset(&pkrs, 0, sizeof(pkrs));
        int rc = g_is_box
            ? bridge_get_box_pokerus(g_box, g_slot, &pkrs)
            : bridge_get_party_pokerus(g_slot, &pkrs);
        if (rc == PKSAVE_OK) {
            g_status = pkrs.status;
            g_strain = pkrs.strain;
            g_days   = pkrs.days;
        }
    }

    display_clear(PAL_BODY);
    pokerus_redraw();
}

static void commit_to_save(void)
{
    if (!bridge_ok()) return;
    uint8_t strain = g_strain;
    uint8_t days   = g_days;

    switch (g_status) {
        case PKSAVE_PKRS_NEVER:
            strain = 0U;
            days   = 0U;
            break;
        case PKSAVE_PKRS_ACTIVE:
            if (strain == 0U) strain = 1U;
            if (days   == 0U) days   = 1U;
            break;
        case PKSAVE_PKRS_CURED:
            if (strain == 0U) strain = 1U;
            days = 0U;
            break;
        default:
            break;
    }

    if (g_is_box) {
        bridge_set_box_pokerus(g_box, g_slot, strain, days);
    } else {
        bridge_set_party_pokerus(g_slot, strain, days);
    }
    bridge_mark_dirty();
}

scene_kind_t pokerus_modal_scene_update(void)
{
    if (input_pressed(J_B)) {
        return SCENE_POKEMON_EDIT;
    }

    if (input_pressed(J_A)) {
        commit_to_save();
        return SCENE_POKEMON_EDIT;
    }

    uint8_t vis = visible_rows();
    if (input_pressed(J_UP) && vis > 1U) {
        g_cursor = (uint8_t)((g_cursor + vis - 1U) % vis);
        pokerus_redraw();
        return SCENE_NONE;
    }
    if (input_pressed(J_DOWN) && vis > 1U) {
        g_cursor = (uint8_t)((g_cursor + 1U) % vis);
        pokerus_redraw();
        return SCENE_NONE;
    }

    if (input_pressed(J_LEFT) || input_pressed(J_RIGHT)) {
        int8_t dir = input_pressed(J_RIGHT) ? 1 : -1;
        if (g_cursor == ROW_STATUS) {
            cycle_status(dir);
        } else if (g_cursor == ROW_STRAIN) {
            step_strain(dir);
        } else if (g_cursor == ROW_DAYS) {
            step_days(dir);
        }
        pokerus_redraw();
    }

    return SCENE_NONE;
}
