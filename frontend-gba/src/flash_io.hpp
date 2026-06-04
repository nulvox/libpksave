// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Raw FLASH 128KB I/O for the editor cart.  Butano's bn::sram only
// addresses the bottom 32KB and assumes SRAM; Gen III Pokemon cartridges
// use 128KB FLASH with bank switching, so we bypass butano and talk to
// the FLASH chip directly.
//
// All accesses are 8-bit (FLASH is byte-addressable) and the command
// sequences execute from IWRAM via the EWRAM_CODE/IWRAM_CODE attributes
// inside flash_io.cpp.  Calling these routines while interrupts that
// touch ROM/SRAM are firing is unsafe; main.cpp calls them only during
// boot and from the save_scene, both with no other I/O outstanding.

#ifndef PKSAVE_GBA_FLASH_IO_HPP
#define PKSAVE_GBA_FLASH_IO_HPP

#include <cstdint>

namespace pksave_gba {

inline constexpr int FLASH_TOTAL_BYTES = 131072;   // 128 KB = two 64 KB banks
inline constexpr int FLASH_BANK_BYTES  = 65536;
inline constexpr int FLASH_BANK_COUNT  = 2;

// Copy all 128 KB of cartridge FLASH into `out` (which must point to at
// least FLASH_TOTAL_BYTES of writable memory).  Switches banks as
// needed.  Returns true on success.
bool flash_read_all(std::uint8_t* out);

// Write all 128 KB of `in` into cartridge FLASH, erasing each bank
// first.  This is destructive and SLOW (full chip erase + reprogram).
// Returns true on success.
bool flash_write_all(const std::uint8_t* in);

}  // namespace pksave_gba

#endif  // PKSAVE_GBA_FLASH_IO_HPP
