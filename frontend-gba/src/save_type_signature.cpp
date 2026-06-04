// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// GBA cartridge save-type detection works by scanning the ROM for one of
// a handful of magic ASCII strings:
//   "EEPROM_V"      — EEPROM (512 B or 8 KB)
//   "SRAM_V"        — SRAM (32 KB)
//   "FLASH_V"       — FLASH 64 KB
//   "FLASH512_V"    — FLASH 64 KB
//   "FLASH1M_V"     — FLASH 128 KB  ← what Gen III Pokemon games use
//
// We need our editor ROM to be detected as FLASH 128KB so the emulator
// allocates a 131,072-byte save backing.  Butano's own SRAM module
// embeds "SRAM_V113"; most emulators (mGBA in particular) prefer FLASH
// when both strings are present, but to be safe we put FLASH1M_V in a
// dedicated `.rodata.save_type` section, marked `used` so the linker
// keeps it.
//
// The trailing "1" gives the conventional version-number suffix.

// devkitARM's gba.specs enables --gc-sections, which would normally
// drop our magic save-type string because nothing inside the C++ TU
// graph references it.  To keep it alive we expose a tiny anchor
// function with extern "C" linkage; main.cpp calls it via a volatile
// pointer at startup, which forces the linker to retain both the
// function and the string it points to.
extern "C" {

// `gnu::used` keeps the symbol from being discarded by the compiler.
[[gnu::used, gnu::section(".rodata.save_type")]]
const char save_type_signature[] = "FLASH1M_V103";

[[gnu::used]]
const char* pksave_save_type_signature_anchor()
{
    return save_type_signature;
}

}  // extern "C"
