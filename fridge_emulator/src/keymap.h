#pragma once

#include <SDL.h>
#include <fridge.h>

// Maps an SDL2 key event to a 7-bit Fridge keyboard code (ASCII subset).
// Returns -1 if the key is not mapped / non-printable.
//
// The Fridge keyboard controller encodes keys via FRIDGE_KEYBOARD_KEY_CODE_MASK
// (0x7f); FRIDGE_keyboard_press() / _release() handle the state bit, so callers
// pass the bare 7-bit code (e.g. 'a', '\n', 0x1b for ESC).
int KeyMap_SdlToCode(const SDL_Event& event);