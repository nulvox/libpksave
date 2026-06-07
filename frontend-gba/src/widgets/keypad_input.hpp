// SPDX-License-Identifier: MIT
// Copyright (c) 2026 gbsdk contributors
//
// Thin keypad-input abstraction so widgets can be unit-tested on the
// host without dragging in butano. The cycler widget calls into this
// namespace instead of touching <bn_keypad.h> directly; under butano
// builds the wrappers compile down to direct bn::keypad calls.
//
// Host tests define PKSAVE_GBA_HOSTTEST_KEYPAD and link against
// frontend-gba/tests/host/keypad_input_host.cpp, which exposes a
// settable state object.

#ifndef PKSAVE_GBA_WIDGETS_KEYPAD_INPUT_HPP
#define PKSAVE_GBA_WIDGETS_KEYPAD_INPUT_HPP

namespace pksave_gba::keypad_input {

bool left_held();
bool right_held();

}  // namespace pksave_gba::keypad_input

#endif  // PKSAVE_GBA_WIDGETS_KEYPAD_INPUT_HPP
