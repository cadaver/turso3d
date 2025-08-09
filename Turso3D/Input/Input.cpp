// For conditions of distribution and use, see copyright notice in License.txt

#include "Input.h"

#include <SDL3/SDL.h>
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
    SDL_SetWindowRelativeMouseMode(window, SDL_FALSE);
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
    textInput.clear();
    shouldExit = false;

    unsigned focusFlags = SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS;
    if (focusFlags && !focus)
    {
        focus = true;
        SDL_SetWindowRelativeMouseMode(window, SDL_TRUE);
        SendEvent(focusGainedEvent);
    }
    if (!focusFlags && focus)
    {
        focus = false;
        keyStates.clear();
        SDL_SetWindowRelativeMouseMode(window, SDL_FALSE);
        SendEvent(focusLostEvent);
    }

    SDL_PumpEvents();
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            shouldExit = true;
            SendEvent(exitRequestEvent);
            break;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.repeat == 0)
                keyStates[event.key.key] = STATE_PRESSED;
            keyPressEvent.keyCode = event.key.key;
            keyPressEvent.repeat = event.key.repeat;
            SendEvent(keyPressEvent);
            break;

        case SDL_EVENT_KEY_UP:
            keyStates[event.key.key] = STATE_RELEASED;
            keyReleaseEvent.keyCode = event.key.key;
            keyReleaseEvent.repeat = false;
            SendEvent(keyReleaseEvent);
            break;

        case SDL_EVENT_TEXT_INPUT:
            textInputEvent.text = std::string(&event.text.text[0]);
            textInput += textInputEvent.text;
            SendEvent(textInputEvent);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            mouseButtonStates[event.button.button] = STATE_PRESSED;
            mousePressEvent.button = event.button.button;
            mousePressEvent.repeat = false;
            SendEvent(mousePressEvent);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            mouseButtonStates[event.button.button] = STATE_RELEASED;
            mouseReleaseEvent.button = event.button.button;
            mouseReleaseEvent.repeat = false;
            SendEvent(mouseReleaseEvent);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (focus)
            {
                mouseMove.x += (int)event.motion.xrel;
                mouseMove.y += (int)event.motion.yrel;
                mouseMoveEvent.delta = IntVector2((int)event.motion.xrel, (int)event.motion.yrel);
                SendEvent(mouseMoveEvent);
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            if (focus)
            {
                mouseWheel.x += (int)event.wheel.x;
                mouseWheel.y += (int)event.wheel.y;
                mouseWheelEvent.delta = IntVector2((int)event.wheel.x, (int)event.wheel.y);
                SendEvent(mouseWheelEvent);
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
