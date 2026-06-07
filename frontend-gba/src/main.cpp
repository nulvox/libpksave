// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// pksave-gba entry point.
//
// Boot sequence:
//   1. bn::core::init()  — butano hardware bring-up.
//   2. flash_read_all()  — copy both 64 KB FLASH banks into the EWRAM
//                          save buffer.
//   3. Construct pksave_bridge — runs pksave_detect, then
//      pksave_open on success.
//   4. Pick initial scene:
//        - bridge unusable  → boot_error_scene with bridge.reason()
//        - bridge OK        → main_menu_scene(bridge)
//   5. Scene loop: each frame call scene->update(); if it returns a
//      new scene_kind, swap the unique_ptr; then bn::core::update().

#include <bn_core.h>
#include <bn_unique_ptr.h>

#include "flash_io.hpp"
#include "pksave_bridge.hpp"
#include "scenes/scene.hpp"
#include "scenes/boot_error_scene.hpp"
#include "scenes/main_menu_scene.hpp"
#include "scenes/trainer_scene.hpp"
#include "scenes/party_scene.hpp"
#include "scenes/pokemon_edit_scene.hpp"
#include "scenes/save_scene.hpp"
#include "widgets/text_input.hpp"

namespace {

// Build the initial scene for a given bridge state.  Pulled into a
// helper so the same selection logic can be reused if Checkpoint #2
// adds a "boot menu" or recovery path.
bn::unique_ptr<pksave_gba::scene> make_initial_scene(pksave_gba::pksave_bridge& bridge)
{
    if (!bridge.ok())
    {
        return bn::unique_ptr<pksave_gba::scene>(
            new pksave_gba::boot_error_scene(bridge.reason()));
    }
    return bn::unique_ptr<pksave_gba::scene>(
        new pksave_gba::main_menu_scene(bridge));
}

// Build a scene from a transition request.  Checkpoint #1 reaches no
// transitions in practice (main_menu's A is a no-op and boot_error
// never returns), but the table is here so adding scenes in #2 just
// means filling in cases.
bn::unique_ptr<pksave_gba::scene> make_scene(
    pksave_gba::scene_kind kind,
    pksave_gba::pksave_bridge& bridge)
{
    using pksave_gba::scene_kind;
    switch (kind)
    {
        case scene_kind::MAIN_MENU:
            return bn::unique_ptr<pksave_gba::scene>(
                new pksave_gba::main_menu_scene(bridge));

        case scene_kind::BOOT_ERROR:
            return bn::unique_ptr<pksave_gba::scene>(
                new pksave_gba::boot_error_scene("Unhandled boot error"));

        case scene_kind::TRAINER:
            return bn::unique_ptr<pksave_gba::scene>(
                new pksave_gba::trainer_scene(bridge));

        case scene_kind::PARTY:
            return bn::unique_ptr<pksave_gba::scene>(
                new pksave_gba::party_scene(bridge));

        case scene_kind::POKEMON_EDIT:
            return bn::unique_ptr<pksave_gba::scene>(
                new pksave_gba::pokemon_edit_scene(bridge));

        case scene_kind::SAVE:
            return bn::unique_ptr<pksave_gba::scene>(
                new pksave_gba::save_scene(bridge));

        case scene_kind::TEXT_INPUT:
            return bn::unique_ptr<pksave_gba::scene>(
                new pksave_gba::text_input_scene());

        // Bag / Boxes land in Checkpoint #4 / #5.  Until then, fall
        // back to the main menu so a stray transition can't strand the
        // user.  `default` satisfies butano's -Wswitch-default warning.
        case scene_kind::BAG:
        case scene_kind::BOXES:
        default:
            return bn::unique_ptr<pksave_gba::scene>(
                new pksave_gba::main_menu_scene(bridge));
    }
}

}  // namespace

// Forward-declared anchor that references the FLASH1M_V save-type
// signature.  Calling it once from main() forces the linker to keep
// the .rodata.save_type section in the final ROM, so emulators and
// flashcarts can detect FLASH 128KB at boot.  See save_type_signature.cpp.
extern "C" const char* pksave_save_type_signature_anchor();

int main()
{
    // Volatile assignment so the optimiser can't dead-code-eliminate
    // the reference and let the linker drop the save-type string.
    volatile const char* anchor = pksave_save_type_signature_anchor();
    (void)anchor;

    bn::core::init();

    // Stream the cartridge FLASH into our 128KB EWRAM mirror.
    pksave_gba::flash_read_all(pksave_gba::save_buffer());

    // pksave_detect / pksave_open both happen inside the bridge's ctor.
    pksave_gba::pksave_bridge bridge;

    bn::unique_ptr<pksave_gba::scene> current = make_initial_scene(bridge);

    while (true)
    {
        bn::optional<pksave_gba::scene_kind> next = current->update();
        if (next.has_value())
        {
            // Reset first so the old scene's VRAM frees before the new
            // one constructs.  bn::unique_ptr's reset(nullptr) calls
            // the destructor.
            current.reset();
            current = make_scene(*next, bridge);
        }
        bn::core::update();
    }
}
