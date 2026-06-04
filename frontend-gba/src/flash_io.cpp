// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// GBA FLASH 128KB protocol implementation.
//
// Reference: GBATEK "GBA Cart Backup Flash ROM" section.
//
// Wire-level protocol (simplified):
//   Read byte:        *(0x0E000000 + offset)
//   Write byte:       command sequence ending in [addr] = value
//   Erase 4KB sector: command sequence ending in [sector] = 0x30
//   Switch bank:      command sequence ending in [0x0000] = bank_id
//
// All writes happen via a 3-write command preamble:
//     [0x5555] = 0xAA
//     [0x2AAA] = 0x55
//     [0x5555] = <command>
//
// FLASH and ROM share the cartridge bus, so write routines must execute
// from IWRAM (not from ROM).  devkitARM's CODE_IN_IWRAM attribute is
// the standard pattern.

#include "flash_io.hpp"

#include <cstddef>
#include <cstdint>

// devkitARM ships these in <gba/gba.h> but butano deliberately doesn't
// expose libgba.  We only need the IWRAM section attribute.  Define it
// locally rather than dragging in the libgba header.
#define IWRAM_CODE __attribute__((section(".iwram"), long_call))

namespace pksave_gba {
namespace {

// Memory-mapped FLASH chip address space.
volatile std::uint8_t* const FLASH_BASE =
    reinterpret_cast<volatile std::uint8_t*>(0x0E000000);

// Wait-state register — same one butano touches for SRAM init.  We
// leave butano's setting alone (BN_CFG_SRAM_WAIT_STATE=WAIT_STATE_3 in
// the Makefile) so this stays empty in production; documented here so
// the next maintainer knows where the timing came from.
//
// REG_WAITCNT @ 0x04000204, bits 0-1: SRAM wait state (us = "3"
// translates to 0b11 = 8 cycles).  Set by butano in bn::hw::sram::init().

// Send the standard 3-byte unlock prefix that precedes every FLASH
// command on Atmel / Sanyo / SST 128KB chips.
IWRAM_CODE inline void flash_unlock_prefix()
{
    FLASH_BASE[0x5555] = 0xAA;
    FLASH_BASE[0x2AAA] = 0x55;
}

// Issue a top-level FLASH command (e.g. erase, program, bank-switch).
IWRAM_CODE inline void flash_command(std::uint8_t cmd)
{
    flash_unlock_prefix();
    FLASH_BASE[0x5555] = cmd;
}

// Switch the visible 64KB bank (0 or 1).
IWRAM_CODE void flash_set_bank(std::uint8_t bank)
{
    flash_command(0xB0);
    FLASH_BASE[0x0000] = bank;
}

// Spin until *addr reads `expected`.  FLASH program / erase operations
// complete asynchronously; the hardware mirrors the in-flight byte at
// the target address, and the operation is "done" when the byte
// matches the value we asked for.  A timeout guard prevents hangs on
// emulators that don't model the chip exactly.
IWRAM_CODE bool flash_poll(volatile std::uint8_t* addr, std::uint8_t expected)
{
    // Polling budget: ~64K iterations is far more than any real
    // operation needs but cheap to burn through.
    for (int i = 0; i < 65536; ++i)
    {
        if (*addr == expected)
        {
            return true;
        }
    }
    return false;
}

// Erase a single 4KB sector.  `offset_in_bank` must be < 64KB and a
// multiple of 0x1000.
IWRAM_CODE bool flash_erase_sector(std::uint16_t offset_in_bank)
{
    flash_command(0x80);
    flash_unlock_prefix();
    FLASH_BASE[offset_in_bank] = 0x30;
    return flash_poll(&FLASH_BASE[offset_in_bank], 0xFF);
}

// Program one byte at `offset_in_bank`.  Sector must already be erased
// (so the previous value is 0xFF).
IWRAM_CODE bool flash_write_byte(std::uint16_t offset_in_bank, std::uint8_t value)
{
    flash_command(0xA0);
    FLASH_BASE[offset_in_bank] = value;
    return flash_poll(&FLASH_BASE[offset_in_bank], value);
}

}  // namespace

bool flash_read_all(std::uint8_t* out)
{
    if (out == nullptr)
    {
        return false;
    }

    for (int bank = 0; bank < FLASH_BANK_COUNT; ++bank)
    {
        flash_set_bank(static_cast<std::uint8_t>(bank));
        const int dst_offset = bank * FLASH_BANK_BYTES;

        // Plain byte-by-byte read.  __agbabi_memcpy1 would be faster
        // but introduces a butano-internal dependency we don't need
        // here — boot copy happens once and is not perf-sensitive.
        for (int i = 0; i < FLASH_BANK_BYTES; ++i)
        {
            out[dst_offset + i] = FLASH_BASE[i];
        }
    }

    // Leave the visible bank back at 0 — standard etiquette.
    flash_set_bank(0);
    return true;
}

bool flash_write_all(const std::uint8_t* in)
{
    if (in == nullptr)
    {
        return false;
    }

    for (int bank = 0; bank < FLASH_BANK_COUNT; ++bank)
    {
        flash_set_bank(static_cast<std::uint8_t>(bank));
        const int src_offset = bank * FLASH_BANK_BYTES;

        // Erase the whole bank, sector by sector (16 × 4KB = 64KB).
        for (std::uint32_t sector = 0; sector < FLASH_BANK_BYTES; sector += 0x1000)
        {
            if (!flash_erase_sector(static_cast<std::uint16_t>(sector)))
            {
                return false;
            }
        }

        // Program every byte that isn't already 0xFF (which it is, post
        // erase, but the conditional saves time on emulators that
        // model write latency).
        for (int i = 0; i < FLASH_BANK_BYTES; ++i)
        {
            const std::uint8_t byte = in[src_offset + i];
            if (byte != 0xFF)
            {
                if (!flash_write_byte(static_cast<std::uint16_t>(i), byte))
                {
                    return false;
                }
            }
        }
    }

    flash_set_bank(0);
    return true;
}

}  // namespace pksave_gba
