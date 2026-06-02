/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 gbsdk contributors
 *
 * Gen 4 save format implementation.
 * TODO(3): implement using pret decomp as reference — see AGENT_PROMPT.md
 *          §Format Research Sources.
 */
#include "pksave_internal.h"

#ifdef PKSAVE_GEN4

int gen4_get_trainer(pksave_save_t *s, pksave_trainer_t *out)
    { (void)s; (void)out; return PKSAVE_ERR_GAME; }
int gen4_set_trainer(pksave_save_t *s, const pksave_trainer_t *in)
    { (void)s; (void)in; return PKSAVE_ERR_GAME; }
int gen4_get_party(pksave_save_t *s, pksave_pokemon_t out[6], uint8_t *count)
    { (void)s; (void)out; (void)count; return PKSAVE_ERR_GAME; }
int gen4_set_party_slot(pksave_save_t *s, uint8_t slot, const pksave_pokemon_t *mon)
    { (void)s; (void)slot; (void)mon; return PKSAVE_ERR_GAME; }
int gen4_get_box(pksave_save_t *s, uint8_t box, pksave_pokemon_t out[30], uint8_t *cnt)
    { (void)s; (void)box; (void)out; (void)cnt; return PKSAVE_ERR_GAME; }
int gen4_set_box_slot(pksave_save_t *s, uint8_t box, uint8_t slot, const pksave_pokemon_t *mon)
    { (void)s; (void)box; (void)slot; (void)mon; return PKSAVE_ERR_GAME; }
int gen4_get_pocket(pksave_save_t *s, pksave_pocket_t p, pksave_item_t *out, uint8_t *cnt)
    { (void)s; (void)p; (void)out; (void)cnt; return PKSAVE_ERR_GAME; }
int gen4_set_item(pksave_save_t *s, pksave_pocket_t p, uint8_t slot, const pksave_item_t *item)
    { (void)s; (void)p; (void)slot; (void)item; return PKSAVE_ERR_GAME; }
int gen4_flush(pksave_save_t *s)
    { (void)s; return PKSAVE_ERR_GAME; }
int gen4_validate_checksum(const uint8_t *sram, size_t len, pksave_game_t game)
    { (void)sram; (void)len; (void)game; return PKSAVE_ERR_GAME; }

#endif /* PKSAVE_GEN4 */
