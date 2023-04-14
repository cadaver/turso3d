// For conditions of distribution and use, see copyright notice in License.txt

#include "Input.h"

#include <SDL.h>
#include <tracy/Tracy.hpp>

Input::Input(SDL_Window* window_) :
    window(window_),
    mouseMove(IntVector2::ZERO),
    mouseWheel(IntVector2::ZERO),
    focus(false),
    shouldExit(false)
{
    assert(window);
}

Input::~Input()
{
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

void Input::Update()
{
    ZoneScoped;

    for (auto it = keyStates.begin(); it != keyStates.end(); ++it)
    {
        if (it->second == STATE_RELEASED)
            it->second = STATE_UP;
        else if (it->second == STATE_PRESSED)
            it->second = STATE_DOWN;
    }
    for (auto it = mouseButtonStates.begin(); it != mouseButtonStates.end(); ++it)
    {
        if (it->second == STATE_RELEASED)
            it->second = STATE_UP;
        else if (it->second == STATE_PRESSED)
            it->second = STATE_DOWN;
    }

    mouseMove = IntVector2::ZERO;
    mouseWheel = IntVector2::ZERO;

    unsigned focusFlags = SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS;
    if (focusFlags && !focus)
    {
        focus = true;
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    if (!focusFlags && focus)
    {
        focus = false;
        keyStates.clear();
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }

    SDL_PumpEvents();
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            shouldExit = true;
            break;

        case SDL_KEYDOWN:
            if (event.key.repeat == 0)
                keyStates[event.key.keysym.sym] = STATE_PRESSED;
            break;

        case SDL_KEYUP:
            keyStates[event.key.keysym.sym] = STATE_RELEASED;
            break;

        case SDL_MOUSEBUTTONDOWN:
            mouseButtonStates[event.button.button] = STATE_PRESSED;
            break;

        case SDL_MOUSEBUTTONUP:
            mouseButtonStates[event.button.button] = STATE_RELEASED;
            break;

        case SDL_MOUSEMOTION:
            if (focus)
            {
                mouseMove.x += event.motion.xrel;
                mouseMove.y += event.motion.yrel;
            }
            break;

        case SDL_MOUSEWHEEL:
            if (focus)
            {
                mouseWheel.x += event.wheel.x;
                mouseWheel.y += event.wheel.y;
            }
            break;
        }
    }
}

ButtonState Input::KeyState(unsigned keyCode) const
{
    auto it = keyStates.find(keyCode);
    return it != keyStates.end() ? it->second : STATE_UP;
}

ButtonState Input::MouseButtonState(unsigned num) const
{
    auto it = mouseButtonStates.find(num);
    return it != mouseButtonStates.end() ? it->second : STATE_UP;
}
