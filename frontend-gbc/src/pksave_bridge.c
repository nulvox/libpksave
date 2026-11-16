/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 */
#include <gb/gb.h>
#include <string.h>
#include "pksave_bridge.h"
#include "scenes.h"

#define ROM_BANK_GEN1   1U
#define ROM_BANK_GEN2   2U
#define ROM_BANK_NAMES  3U
#define ROM_BANK_RECALC 4U

#define GBC_SRAM_WINDOW ((uint8_t *)0xA000U)
#define GBC_SRAM_LEN    0x8000U

pksave_pokemon_t     g_party_buf[6];
pksave_pokemon_t     g_box_buf[30];
pksave_item_t        g_bag_buf[60];
text_input_mailbox_t g_text_input;

static pksave_save_t *g_handle             = NULL;
static pksave_game_t  g_game               = PKSAVE_GAME_UNKNOWN;
static uint8_t        g_ok                 = 0U;
static uint8_t        g_dirty              = 0U;
static int8_t         g_pending_party_slot = -1;
static int8_t         g_pending_box_index  = -1;
static int8_t         g_pending_box_slot   = -1;
static pksave_pocket_t g_pending_bag_pocket = PKSAVE_POCKET_NONE;
static int8_t         g_pending_bag_slot   = -1;
static char           g_name_buf[16];

static uint8_t codec_bank(void)
{
    return (g_game == PKSAVE_GAME_RB || g_game == PKSAVE_GAME_YELLOW)
         ? ROM_BANK_GEN1
         : ROM_BANK_GEN2;
}

static void copy_name_to_wram(const char *src)
{
    uint8_t i = 0;
    if (!src) {
        g_name_buf[0] = '\0';
        return;
    }
    while (i + 1U < sizeof(g_name_buf) && src[i] != '\0') {
        g_name_buf[i] = src[i];
        i++;
    }
    g_name_buf[i] = '\0';
}

void bridge_init(void)
{
    uint8_t saved = _current_bank;
    ENABLE_RAM;

    SWITCH_ROM(ROM_BANK_RECALC);
    g_game = pksave_detect(GBC_SRAM_WINDOW, GBC_SRAM_LEN);
    if (g_game != PKSAVE_GAME_UNKNOWN) {
        g_handle = pksave_open(GBC_SRAM_WINDOW, GBC_SRAM_LEN, g_game);
        g_ok     = (g_handle != NULL) ? 1U : 0U;
    } else {
        g_handle = NULL;
        g_ok     = 0U;
    }
    g_dirty              = 0U;
    g_pending_party_slot = -1;
    g_pending_box_index  = -1;
    g_pending_box_slot   = -1;
    g_pending_bag_pocket = PKSAVE_POCKET_NONE;
    g_pending_bag_slot   = -1;
    memset(&g_text_input, 0, sizeof(g_text_input));

    SWITCH_ROM(saved);
}

uint8_t bridge_ok(void)
{
    return g_ok;
}

pksave_game_t bridge_game(void)
{
    return g_game;
}

uint8_t bridge_is_gen1(void)
{
    return (g_game == PKSAVE_GAME_RB || g_game == PKSAVE_GAME_YELLOW) ? 1U : 0U;
}

uint8_t bridge_is_gen2(void)
{
    return (g_game == PKSAVE_GAME_GS || g_game == PKSAVE_GAME_CRYSTAL) ? 1U : 0U;
}

const char *bridge_game_display_name(void)
{
    switch (g_game) {
        case PKSAVE_GAME_RB:      return "POKEMON RED/BLUE";
        case PKSAVE_GAME_YELLOW:  return "POKEMON YELLOW";
        case PKSAVE_GAME_GS:      return "POKEMON GOLD/SILV";
        case PKSAVE_GAME_CRYSTAL: return "POKEMON CRYSTAL";
        default:                  return "UNKNOWN SAVE";
    }
}

uint8_t bridge_is_dirty(void)
{
    return g_dirty;
}

void bridge_mark_dirty(void)
{
    g_dirty = 1U;
}

void bridge_clear_dirty(void)
{
    g_dirty = 0U;
}

int8_t bridge_pending_party_slot(void)
{
    return g_pending_party_slot;
}

void bridge_set_pending_party_slot(int8_t slot)
{
    g_pending_party_slot = slot;
}

int8_t bridge_pending_box_index(void)
{
    return g_pending_box_index;
}

int8_t bridge_pending_box_slot(void)
{
    return g_pending_box_slot;
}

void bridge_set_pending_box(int8_t box_index, int8_t slot)
{
    g_pending_box_index = box_index;
    g_pending_box_slot  = slot;
}

void bridge_clear_pending_box(void)
{
    g_pending_box_index = -1;
    g_pending_box_slot  = -1;
}

pksave_pocket_t bridge_pending_bag_pocket(void)
{
    return g_pending_bag_pocket;
}

int8_t bridge_pending_bag_slot(void)
{
    return g_pending_bag_slot;
}

void bridge_set_pending_bag(pksave_pocket_t pocket, int8_t slot)
{
    g_pending_bag_pocket = pocket;
    g_pending_bag_slot   = slot;
}

void bridge_clear_pending_bag(void)
{
    g_pending_bag_pocket = PKSAVE_POCKET_NONE;
    g_pending_bag_slot   = -1;
}

/* ── Bank-switched libpksave wrappers ───────────────────────────────────── */

int bridge_get_trainer(pksave_trainer_t *out)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_get_trainer(g_handle, out);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_set_trainer(const pksave_trainer_t *in)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_set_trainer(g_handle, in);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_get_party(pksave_pokemon_t out[6], uint8_t *count)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_get_party(g_handle, out, count);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_set_party_slot(uint8_t slot, const pksave_pokemon_t *mon)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_set_party_slot(g_handle, slot, mon);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_get_box_count(void)
{
    return pksave_get_box_count(g_handle);
}

int bridge_get_box(uint8_t box, pksave_pokemon_t out[30], uint8_t *count)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_get_box(g_handle, box, out, count);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_set_box_slot(uint8_t box, uint8_t slot, const pksave_pokemon_t *mon)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_set_box_slot(g_handle, box, slot, mon);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_get_pocket(pksave_pocket_t pocket, pksave_item_t *out, uint8_t *count)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_get_pocket(g_handle, pocket, out, count);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_set_item(pksave_pocket_t pocket, uint8_t slot, const pksave_item_t *item)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_set_item(g_handle, pocket, slot, item);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_get_party_pokerus(uint8_t slot, pksave_pokerus_t *out)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_get_party_pokerus(g_handle, slot, out);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_set_party_pokerus(uint8_t slot, uint8_t strain, uint8_t days)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_set_party_pokerus(g_handle, slot, strain, days);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_get_box_pokerus(uint8_t box, uint8_t slot, pksave_pokerus_t *out)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_get_box_pokerus(g_handle, box, slot, out);
    SWITCH_ROM(saved);
    return rc;
}

int bridge_set_box_pokerus(uint8_t box, uint8_t slot, uint8_t strain, uint8_t days)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_set_box_pokerus(g_handle, box, slot, strain, days);
    SWITCH_ROM(saved);
    return rc;
}

void bridge_recalc_stats(pksave_pokemon_t *pkm)
{
    uint8_t saved = _current_bank;
    SWITCH_ROM(ROM_BANK_RECALC);
    pksave_recalc_stats(pkm, g_game);
    SWITCH_ROM(saved);
}

int bridge_flush(void)
{
    uint8_t saved = _current_bank;
    int rc;
    SWITCH_ROM(codec_bank());
    rc = pksave_flush(g_handle);
    SWITCH_ROM(saved);
    return rc;
}

const char *bridge_species_name(uint16_t species_id)
{
    uint8_t saved = _current_bank;
    SWITCH_ROM(ROM_BANK_NAMES);
    copy_name_to_wram(pksave_species_name(g_game, species_id));
    SWITCH_ROM(saved);
    return g_name_buf;
}

const char *bridge_move_name(uint16_t move_id)
{
    uint8_t saved = _current_bank;
    SWITCH_ROM(ROM_BANK_NAMES);
    copy_name_to_wram(pksave_move_name(g_game, move_id));
    SWITCH_ROM(saved);
    return g_name_buf;
}

const char *bridge_item_name(uint16_t item_id)
{
    uint8_t saved = _current_bank;
    SWITCH_ROM(ROM_BANK_NAMES);
    copy_name_to_wram(pksave_item_name(g_game, item_id));
    SWITCH_ROM(saved);
    return g_name_buf;
}

uint8_t bridge_move_base_pp(uint16_t move_id)
{
    uint8_t saved = _current_bank;
    uint8_t pp;
    SWITCH_ROM(ROM_BANK_NAMES);
    pp = pksave_move_base_pp(g_game, move_id);
    SWITCH_ROM(saved);
    return pp;
}

uint16_t bridge_item_max_stack(uint16_t item_id)
{
    uint8_t saved = _current_bank;
    uint16_t cap;
    SWITCH_ROM(ROM_BANK_NAMES);
    cap = pksave_item_max_stack(g_game, item_id);
    SWITCH_ROM(saved);
    return cap;
}

pksave_pocket_t bridge_item_pocket(uint16_t item_id)
{
    uint8_t saved = _current_bank;
    pksave_pocket_t p;
    SWITCH_ROM(ROM_BANK_NAMES);
    p = pksave_item_pocket(g_game, item_id);
    SWITCH_ROM(saved);
    return p;
}

uint8_t bridge_iv_max(void)
{
    uint8_t saved = _current_bank;
    uint8_t cap;
    SWITCH_ROM(ROM_BANK_RECALC);
    cap = pksave_iv_max(g_game);
    SWITCH_ROM(saved);
    return cap;
}

uint16_t bridge_ev_max_per_stat(void)
{
    uint8_t saved = _current_bank;
    uint16_t cap;
    SWITCH_ROM(ROM_BANK_RECALC);
    cap = pksave_ev_max_per_stat(g_game);
    SWITCH_ROM(saved);
    return cap;
}
