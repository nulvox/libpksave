/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Per-game numeric limits (EV/IV caps).  These are constant data — no
 * gen-conditional compilation needed; the switch dispatches at runtime.
 */
#include "pksave.h"

uint8_t pksave_iv_max(pksave_game_t game)
{
    switch (game) {
    case PKSAVE_GAME_RS:
    case PKSAVE_GAME_EMERALD:
    case PKSAVE_GAME_FRLG:
    case PKSAVE_GAME_DP:
    case PKSAVE_GAME_PLATINUM:
    case PKSAVE_GAME_HGSS:
    case PKSAVE_GAME_BW:
    case PKSAVE_GAME_BW2:
        return 31;
    case PKSAVE_GAME_RB:
    case PKSAVE_GAME_YELLOW:
    case PKSAVE_GAME_GS:
    case PKSAVE_GAME_CRYSTAL:
        return 15;
    default:
        return 0;
    }
}

uint16_t pksave_ev_max_per_stat(pksave_game_t game)
{
    switch (game) {
    case PKSAVE_GAME_RS:
    case PKSAVE_GAME_EMERALD:
    case PKSAVE_GAME_FRLG:
    case PKSAVE_GAME_DP:
    case PKSAVE_GAME_PLATINUM:
    case PKSAVE_GAME_HGSS:
    case PKSAVE_GAME_BW:
    case PKSAVE_GAME_BW2:
        return 252;
    case PKSAVE_GAME_RB:
    case PKSAVE_GAME_YELLOW:
    case PKSAVE_GAME_GS:
    case PKSAVE_GAME_CRYSTAL:
        return 65535;
    default:
        return 0;
    }
}

uint16_t pksave_ev_total_max(pksave_game_t game)
{
    switch (game) {
    case PKSAVE_GAME_RS:
    case PKSAVE_GAME_EMERALD:
    case PKSAVE_GAME_FRLG:
    case PKSAVE_GAME_DP:
    case PKSAVE_GAME_PLATINUM:
    case PKSAVE_GAME_HGSS:
    case PKSAVE_GAME_BW:
    case PKSAVE_GAME_BW2:
        return 510;
    default:
        return 0;
    }
}
