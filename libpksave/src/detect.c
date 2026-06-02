/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * TODO(3): implement detection signals for each game variant.
 * See SPEC.md §Detection for the signal table and AGENT_PROMPT.md §Format
 * Research Sources for the pret decomp references.
 */
#include "pksave_internal.h"

pksave_game_t pksave_detect(const uint8_t *sram, size_t len)
{
    if (!sram) return PKSAVE_GAME_UNKNOWN;
    (void)len;
    return PKSAVE_GAME_UNKNOWN; /* TODO(3): implement */
}
