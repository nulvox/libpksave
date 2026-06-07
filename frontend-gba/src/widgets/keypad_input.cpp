// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors

#include "keypad_input.hpp"

#include <bn_keypad.h>

namespace pksave_gba::keypad_input {

bool left_held()  { return bn::keypad::left_held(); }
bool right_held() { return bn::keypad::right_held(); }

}  // namespace pksave_gba::keypad_input
