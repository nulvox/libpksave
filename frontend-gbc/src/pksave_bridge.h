/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * ROM-bank-aware bridge between frontend-gbc scenes and libpksave.
 * Lives in ROM Bank 0 so any banked scene (Bank 5 / Bank 6) can call it.
 */
#ifndef PKSAVE_GBC_BRIDGE_H
#define PKSAVE_GBC_BRIDGE_H

#include <stdint.h>
#include "pksave.h"

/* Shared static buffers in WRAM (never allocate large arrays on SM83 stack) */
extern pksave_pokemon_t g_party_buf[6];
extern pksave_pokemon_t g_box_buf[30];
extern pksave_item_t    g_bag_buf[60];

void          bridge_init(void);
uint8_t       bridge_ok(void);
pksave_game_t bridge_game(void);
uint8_t       bridge_is_gen1(void);
uint8_t       bridge_is_gen2(void);
const char   *bridge_game_display_name(void);

uint8_t       bridge_is_dirty(void);
void          bridge_mark_dirty(void);
void          bridge_clear_dirty(void);

/* Scene handoff state */
int8_t          bridge_pending_party_slot(void);
void            bridge_set_pending_party_slot(int8_t slot);

int8_t          bridge_pending_box_index(void);
int8_t          bridge_pending_box_slot(void);
void            bridge_set_pending_box(int8_t box_index, int8_t slot);
void            bridge_clear_pending_box(void);

pksave_pocket_t bridge_pending_bag_pocket(void);
int8_t          bridge_pending_bag_slot(void);
void            bridge_set_pending_bag(pksave_pocket_t pocket, int8_t slot);
void            bridge_clear_pending_bag(void);

/* Bank-switched libpksave wrappers */
int             bridge_get_trainer(pksave_trainer_t *out);
int             bridge_set_trainer(const pksave_trainer_t *in);

int             bridge_get_party(pksave_pokemon_t out[6], uint8_t *count);
int             bridge_set_party_slot(uint8_t slot, const pksave_pokemon_t *mon);

int             bridge_get_box_count(void);
int             bridge_get_box(uint8_t box, pksave_pokemon_t out[30], uint8_t *count);
int             bridge_set_box_slot(uint8_t box, uint8_t slot, const pksave_pokemon_t *mon);

int             bridge_get_pocket(pksave_pocket_t pocket, pksave_item_t *out, uint8_t *count);
int             bridge_set_item(pksave_pocket_t pocket, uint8_t slot, const pksave_item_t *item);

int             bridge_get_party_pokerus(uint8_t slot, pksave_pokerus_t *out);
int             bridge_set_party_pokerus(uint8_t slot, uint8_t strain, uint8_t days);
int             bridge_get_box_pokerus(uint8_t box, uint8_t slot, pksave_pokerus_t *out);
int             bridge_set_box_pokerus(uint8_t box, uint8_t slot, uint8_t strain, uint8_t days);

void            bridge_recalc_stats(pksave_pokemon_t *pkm);
int             bridge_flush(void);

/* Name & metadata lookups (returned string is copied into static WRAM) */
const char     *bridge_species_name(uint16_t species_id);
const char     *bridge_move_name(uint16_t move_id);
const char     *bridge_item_name(uint16_t item_id);

uint8_t         bridge_move_base_pp(uint16_t move_id);
uint16_t        bridge_item_max_stack(uint16_t item_id);
pksave_pocket_t bridge_item_pocket(uint16_t item_id);
uint8_t         bridge_iv_max(void);
uint16_t        bridge_ev_max_per_stat(void);

#endif /* PKSAVE_GBC_BRIDGE_H */
