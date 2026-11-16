/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * Scene identifiers, text-input mailbox, and per-scene entry points.
 */
#ifndef PKSAVE_GBC_SCENES_H
#define PKSAVE_GBC_SCENES_H

#include <stdint.h>

typedef enum {
    SCENE_NONE = 0,
    SCENE_BOOT_ERROR,
    SCENE_MAIN_MENU,
    SCENE_TRAINER,
    SCENE_TEXT_INPUT,
    SCENE_PARTY,
    SCENE_BOXES,
    SCENE_POKEMON_EDIT,
    SCENE_POKERUS_MODAL,
    SCENE_BAG,
    SCENE_SLOT_EDIT,
    SCENE_SAVE,
} scene_kind_t;

/* ── Text-input mailbox (shared between callers and text_input_scene) ───── */
typedef struct {
    char         initial[13];
    uint8_t      max_len;
    scene_kind_t return_to;
    uint8_t      caller_tag;
    char         result_value[13];
    uint8_t      result_valid;
    uint8_t      result_cancelled;
    uint8_t      result_tag;
} text_input_mailbox_t;

extern text_input_mailbox_t g_text_input;

/* ── Bank 5 scenes ──────────────────────────────────────────────────────── */
void         boot_error_scene_init(void);
scene_kind_t boot_error_scene_update(void);

void         main_menu_scene_init(void);
scene_kind_t main_menu_scene_update(void);

void         trainer_scene_init(void);
scene_kind_t trainer_scene_update(void);

void         text_input_scene_init(void);
scene_kind_t text_input_scene_update(void);

void         save_scene_init(void);
scene_kind_t save_scene_update(void);

void         bag_scene_init(void);
scene_kind_t bag_scene_update(void);

void         slot_edit_scene_init(void);
scene_kind_t slot_edit_scene_update(void);

/* ── Bank 6 scenes ──────────────────────────────────────────────────────── */
void         party_scene_init(void);
scene_kind_t party_scene_update(void);

void         box_scene_init(void);
scene_kind_t box_scene_update(void);

void         pokemon_edit_scene_init(void);
scene_kind_t pokemon_edit_scene_update(void);

void         pokerus_modal_scene_init(void);
scene_kind_t pokerus_modal_scene_update(void);

#endif /* PKSAVE_GBC_SCENES_H */
