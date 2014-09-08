// For conditions of distribution and use, see copyright notice in License.txt

#include "Input.h"
#include "Window.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Input::Input() :
    mouseButtons(0),
    mouseButtonsPressed(0),
    discardMouseMove(true)
{
    RegisterSubsystem(this);
}

Input::~Input()
{
    RemoveSubsystem(this);
}

void Input::Update()
{
    // Clear accumulated input & key/button presses
    mouseButtonsPressed = 0;
    mouseMove = IntVector2::ZERO;
    keyPressed.Clear();
    rawKeyPressed.Clear();

    // The OS-specific window message handling will call back to Input and update the state
    Window* window = Subsystem<Window>();
    if (window)
        window->PumpMessages();
}

bool Input::KeyDown(unsigned keyCode) const
{
    HashMap<unsigned, bool>::ConstIterator it = keyDown.Find(keyCode);
    return it != keyDown.End() ? it->second : false;
}

bool Input::KeyDownRaw(unsigned rawKeyCode) const
{
    HashMap<unsigned, bool>::ConstIterator it = rawKeyDown.Find(rawKeyCode);
    return it != rawKeyDown.End() ? it->second : false;
}

bool Input::KeyPressed(unsigned keyCode) const
{
    HashMap<unsigned, bool>::ConstIterator it = keyPressed.Find(keyCode);
    return it != keyPressed.End() ? it->second : false;
}

bool Input::KeyPressedRaw(unsigned rawKeyCode) const
{
    HashMap<unsigned, bool>::ConstIterator it = rawKeyPressed.Find(rawKeyCode);
    return it != rawKeyPressed.End() ? it->second : false;
}

bool Input::MouseButtonDown(unsigned button) const
{
    return (mouseButtons & (1 << button)) != 0;
}

bool Input::MouseButtonPressed(unsigned button) const
{
    return (mouseButtonsPressed & (1 << button)) != 0;
}

void Input::OnKey(unsigned keyCode, unsigned rawKeyCode, bool pressed)
{
    bool wasDown = KeyDown(keyCode);

    keyDown[keyCode] = pressed;
    rawKeyDown[rawKeyCode] = pressed;
    if (pressed)
    {
        keyPressed[keyCode] = true;
        rawKeyPressed[rawKeyCode] = true;
    }

    keyEvent.keyCode = keyCode;
    keyEvent.rawKeyCode = rawKeyCode;
    keyEvent.pressed = pressed;
    keyEvent.repeat = wasDown;
    SendEvent(keyEvent);
}

void Input::OnChar(unsigned unicodeChar)
{
    charInputEvent.unicodeChar = unicodeChar;
    SendEvent(charInputEvent);
}

void Input::OnMouseMove(const IntVector2& position)
{
    IntVector2 delta = discardMouseMove ? IntVector2::ZERO : position - mousePosition;
    mousePosition = position;
    mouseMove += delta;
    discardMouseMove = false;

    mouseMoveEvent.position = position;
    mouseMoveEvent.delta = delta;
    SendEvent(mouseMoveEvent);
}

void Input::OnMouseButton(unsigned button, bool pressed)
{
    unsigned bit = 1 << button;

    if (pressed)
    {
        mouseButtons |= bit;
        mouseButtonsPressed |= bit;
    }
    else
        mouseButtons &= ~bit;

    mouseButtonEvent.button = button;
    mouseButtonEvent.buttons = mouseButtons;
    mouseButtonEvent.pressed = pressed;
    mouseButtonEvent.position = mousePosition;
    SendEvent(mouseButtonEvent);
}

void Input::OnGainFocus()
{
    discardMouseMove = true;
}

void Input::OnLoseFocus()
{
    mouseButtons = 0;
    mouseButtonsPressed = 0;
    mouseMove = IntVector2::ZERO;
    keyDown.Clear();
    keyPressed.Clear();
    rawKeyDown.Clear();
    rawKeyPressed.Clear();
}

}
