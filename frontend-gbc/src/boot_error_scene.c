/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include "scenes.h"
#include "display.h"

void boot_error_scene_init(void)
{
    display_clear(PAL_BODY);
    display_draw_row(0U, " SAVE ERROR         ", PAL_ERROR);
    display_draw_row(4U, "  No valid Gen I/II ", PAL_BODY);
    display_draw_row(5U, "  Pokemon save data ", PAL_BODY);
    display_draw_row(6U, "  detected in SRAM. ", PAL_BODY);
    display_draw_row(9U, "  Check .sav symlink", PAL_BODY);
    display_draw_row(10U,"  and restart.      ", PAL_BODY);
    display_draw_footer(" HALTED");
}

scene_kind_t boot_error_scene_update(void)
{
    return SCENE_NONE;
}
