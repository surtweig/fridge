#include "keymap.h"

#include <cctype>

static int CodeForSDLKey(SDL_Keycode k, Uint16 mod)
{
    // ESC, Enter, Backspace, Tab are the most common control codes.
    switch (k)
    {
        case SDLK_ESCAPE:    return 0x1b;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:  return '\n';
        case SDLK_BACKSPACE: return 0x08;
        case SDLK_TAB:       return '\t';
        case SDLK_SPACE:     return ' ';
        default:             break;
    }

    // Printable ASCII (32..126). Use shifted sense for letters.
    if (k >= SDLK_a && k <= SDLK_z)
    {
        bool caps = (mod & (KMOD_SHIFT)) != 0;
        bool capslock = (mod & KMOD_CAPS) != 0;
        if (caps ^ capslock)
            return k - SDLK_a + 'A';
        return k - SDLK_a + 'a';
    }

    if (k >= SDLK_0 && k <= SDLK_9)
    {
        char base = (char)(k - SDLK_0 + '0');
        if (mod & KMOD_SHIFT)
        {
            static const char shifted[] = ")!@#$%^&*(";
            return (int)(unsigned char)shifted[base - '0'];
        }
        return base;
    }

    if (k >= SDLK_KP_1 && k <= SDLK_KP_9)
        return k - SDLK_KP_1 + '1';
    if (k == SDLK_KP_0)
        return '0';

    // Punctuation: SDL keycodes happen to be ASCII for many of these.
    if (k >= 32 && k <= 126)
        return (int)k;

    return -1;
}

int KeyMap_SdlToCode(const SDL_Event& event)
{
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
        return -1;

    int code = CodeForSDLKey(event.key.keysym.sym, event.key.keysym.mod);
    return code;
}