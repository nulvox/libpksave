// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "pksave_bridge.hpp"
#include "flash_io.hpp"

#include <cstdint>

namespace pksave_gba {
namespace {

// 128KB save buffer lives in EWRAM.  Butano's linker script places
// default `.bss` in IWRAM (which is only 32KB), so we need to
// explicitly land this in the EWRAM-bss section that butano sets up
// via -DBN_EWRAM_BSS_SECTION=".sbss" in BN_TOOLCHAIN_CFLAGS.  Butano
// exposes this as the BN_DATA_EWRAM_BSS macro (in bn_hw_common.h);
// we declare it locally to avoid pulling a butano-internal "hw"
// header into a bridge file.
[[gnu::section(".sbss")]]
alignas(4) std::uint8_t g_save_buffer[FLASH_TOTAL_BYTES];

}  // namespace

std::uint8_t* save_buffer()
{
    return g_save_buffer;
}

pksave_bridge::pksave_bridge()
    : handle_(nullptr)
    , game_(PKSAVE_GAME_UNKNOWN)
    , reason_(nullptr)
    , dirty_(false)
{
    game_ = ::pksave_detect(g_save_buffer, FLASH_TOTAL_BYTES);

    if (game_ == PKSAVE_GAME_UNKNOWN)
    {
        reason_ = "Unknown save format";
        return;
    }

    // Refuse anything other than Gen III for this build — the editor is
    // compiled with -DPKSAVE_GEN3 and the higher-gen code paths aren't
    // linked in.  This catches the case of someone symlinking a Gen II
    // GBC save onto the FLASH backing.
    if (game_ != PKSAVE_GAME_RS &&
        game_ != PKSAVE_GAME_EMERALD &&
        game_ != PKSAVE_GAME_FRLG)
    {
        reason_ = "Not a Gen III save";
        return;
    }

    handle_ = ::pksave_open(g_save_buffer, FLASH_TOTAL_BYTES, game_);
    if (handle_ == nullptr)
    {
        reason_ = "Save corrupt";
    }
}

pksave_bridge::~pksave_bridge()
{
    if (handle_ != nullptr)
    {
        ::pksave_close(handle_);
        handle_ = nullptr;
    }
}

bool pksave_bridge::flush_to_flash()
{
    if (handle_ == nullptr)
    {
        return false;
    }

    if (::pksave_flush(handle_) != PKSAVE_OK)
    {
        return false;
    }

    if (!flash_write_all(g_save_buffer))
    {
        return false;
    }

    dirty_ = false;
    return true;
}

const char* game_display_name(pksave_game_t game)
{
    switch (game)
    {
        case PKSAVE_GAME_RS:      return "Ruby/Sapphire";
        case PKSAVE_GAME_EMERALD: return "Emerald";
        case PKSAVE_GAME_FRLG:    return "FireRed/LeafGreen";
        default:                  return "Unknown";
    }
}

}  // namespace pksave_gba
