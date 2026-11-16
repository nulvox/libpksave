/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include "scenes.h"
#include "display.h"
#include "pksave_bridge.h"

#define SAVE_SPLASH_FRAMES 20U

static uint8_t g_save_frame = 0U;
static uint8_t g_save_ok    = 0U;

void save_scene_init(void)
{
    g_save_frame = 0U;
    g_save_ok    = 0U;

    display_clear(PAL_BODY);
    display_draw_header("SAVING", bridge_is_dirty());
    display_draw_row(7U,  "     SAVING...      ", PAL_CURSOR);
    display_draw_row(9U,  "   DON'T TURN OFF   ", PAL_BODY);
    display_draw_row(10U, "     THE POWER!     ", PAL_BODY);
    display_draw_footer(" PLEASE WAIT...");
}

scene_kind_t save_scene_update(void)
{
    /* Frame 0: let the splash screen render for at least one frame first. */
    if (g_save_frame == 0U) {
        g_save_frame = 1U;
        return SCENE_NONE;
    }

    if (g_save_frame == 1U) {
        int rc = bridge_flush();
        if (rc == PKSAVE_OK) {
            bridge_clear_dirty();
            g_save_ok = 1U;
        } else {
            g_save_ok = 0U;
            display_draw_row(0U,  " SAVE FAILED        ", PAL_ERROR);
            display_draw_row(7U,  "    SAVE FAILED.    ", PAL_ERROR);
            display_draw_row(9U,  "  Power off & retry ", PAL_BODY);
            display_draw_row(10U, "                    ", PAL_BODY);
        }
        g_save_frame = 2U;
        return SCENE_NONE;
    }

    if (!g_save_ok) {
        /* Error halt: stay on error message. */
        return SCENE_NONE;
    }

    if (g_save_frame < SAVE_SPLASH_FRAMES) {
        g_save_frame++;
        return SCENE_NONE;
    }

    return SCENE_MAIN_MENU;
}
