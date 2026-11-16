/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * pksave-gbc entry point.
 */
#include <gb/gb.h>
#include "display.h"
#include "input.h"
#include "pksave_bridge.h"
#include "scenes.h"

#define ROM_BANK_SCENES_A 5U
#define ROM_BANK_SCENES_B 6U

static scene_kind_t g_current_scene = SCENE_MAIN_MENU;

static uint8_t scene_rom_bank(scene_kind_t kind)
{
    switch (kind) {
        case SCENE_PARTY:
        case SCENE_BOXES:
        case SCENE_POKEMON_EDIT:
        case SCENE_POKERUS_MODAL:
            return ROM_BANK_SCENES_B;
        default:
            return ROM_BANK_SCENES_A;
    }
}

static void dispatch_scene_init(scene_kind_t kind)
{
    SWITCH_ROM(scene_rom_bank(kind));
    switch (kind) {
        case SCENE_BOOT_ERROR:    boot_error_scene_init();    break;
        case SCENE_MAIN_MENU:     main_menu_scene_init();     break;
        case SCENE_TRAINER:       trainer_scene_init();       break;
        case SCENE_TEXT_INPUT:    text_input_scene_init();    break;
        case SCENE_SAVE:          save_scene_init();          break;
        case SCENE_BAG:           bag_scene_init();           break;
        case SCENE_SLOT_EDIT:     slot_edit_scene_init();     break;
        case SCENE_PARTY:         party_scene_init();         break;
        case SCENE_BOXES:         box_scene_init();           break;
        case SCENE_POKEMON_EDIT:  pokemon_edit_scene_init();  break;
        case SCENE_POKERUS_MODAL: pokerus_modal_scene_init(); break;
        default:                  main_menu_scene_init();     break;
    }
}

static scene_kind_t dispatch_scene_update(scene_kind_t kind)
{
    SWITCH_ROM(scene_rom_bank(kind));
    switch (kind) {
        case SCENE_BOOT_ERROR:    return boot_error_scene_update();
        case SCENE_MAIN_MENU:     return main_menu_scene_update();
        case SCENE_TRAINER:       return trainer_scene_update();
        case SCENE_TEXT_INPUT:    return text_input_scene_update();
        case SCENE_SAVE:          return save_scene_update();
        case SCENE_BAG:           return bag_scene_update();
        case SCENE_SLOT_EDIT:     return slot_edit_scene_update();
        case SCENE_PARTY:         return party_scene_update();
        case SCENE_BOXES:         return box_scene_update();
        case SCENE_POKEMON_EDIT:  return pokemon_edit_scene_update();
        case SCENE_POKERUS_MODAL: return pokerus_modal_scene_update();
        default:                  return SCENE_NONE;
    }
}

void main(void)
{
    display_init();
    bridge_init();

    g_current_scene = bridge_ok() ? SCENE_MAIN_MENU : SCENE_BOOT_ERROR;
    dispatch_scene_init(g_current_scene);

    while (1) {
        input_poll();
        scene_kind_t next = dispatch_scene_update(g_current_scene);
        if (next != SCENE_NONE) {
            g_current_scene = next;
            dispatch_scene_init(g_current_scene);
        }
        wait_vbl_done();
    }
}
