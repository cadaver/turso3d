// For conditions of distribution and use, see copyright notice in License.txt

#include "Input.h"
#include "Window.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Input::Input() :
    mouseButtons(0),
    mouseButtonsPressed(0)
{
    RegisterSubsystem(this);
}

Input::~Input()
{
    RemoveSubsystem(this);
}

void Input::Update()
{
    // Clear accumulated input from last frame
    mouseButtonsPressed = 0;
    mouseMove = IntVector2::ZERO;
    keyPressed.Clear();
    rawKeyPress.Clear();
    for (auto it = touches.Begin(); it != touches.End(); ++it)
        it->delta = IntVector2::ZERO;

    // The OS-specific window message handling will call back to Input and update the state
    Window* window = Subsystem<Window>();
    if (window)
        window->PumpMessages();
}

bool Input::IsKeyDown(unsigned keyCode) const
{
    auto it = keyDown.Find(keyCode);
    return it != keyDown.End() ? it->second : false;
}

bool Input::IsKeyDownRaw(unsigned rawKeyCode) const
{
    auto it = rawKeyDown.Find(rawKeyCode);
    return it != rawKeyDown.End() ? it->second : false;
}

bool Input::IsKeyPress(unsigned keyCode) const
{
    auto it = keyPressed.Find(keyCode);
    return it != keyPressed.End() ? it->second : false;
}

bool Input::IsKeyPressRaw(unsigned rawKeyCode) const
{
    auto it = rawKeyPress.Find(rawKeyCode);
    return it != rawKeyPress.End() ? it->second : false;
}

const IntVector2& Input::MousePosition() const
{
    Window* window = Subsystem<Window>();
    return window ? window->MousePosition() : IntVector2::ZERO;
}

bool Input::IsMouseButtonDown(unsigned button) const
{
    return (mouseButtons & (1 << button)) != 0;
}

bool Input::IsMouseButtonPress(unsigned button) const
{
    return (mouseButtonsPressed & (1 << button)) != 0;
}

const Touch* Input::FindTouch(unsigned id) const
{
    for (auto it = touches.Begin(); it != touches.End(); ++it)
    {
        if (it->id == id)
            return it.ptr;
    }

    return nullptr;
}

void Input::OnKey(unsigned keyCode, unsigned rawKeyCode, bool pressed)
{
    bool wasDown = IsKeyDown(keyCode);

    keyDown[keyCode] = pressed;
    rawKeyDown[rawKeyCode] = pressed;
    if (pressed)
    {
        keyPressed[keyCode] = true;
        rawKeyPress[rawKeyCode] = true;
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

void Input::OnMouseMove(const IntVector2& position, const IntVector2& delta)
{
    mouseMove += delta;

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
    mouseButtonEvent.position = MousePosition();
    SendEvent(mouseButtonEvent);
}

void Input::OnTouch(unsigned internalId, bool pressed, const IntVector2& position, float pressure)
{
    if (pressed)
    {
        bool found = false;

        // Ongoing touch
        for (auto it = touches.Begin(); it != touches.End(); ++it)
        {
            if (it->internalId == internalId)
            {
                found = true;
                it->lastDelta = position - it->position;
                
                if (it->lastDelta != IntVector2::ZERO || pressure != it->pressure)
                {
                    it->delta += it->lastDelta;
                    it->position = position;
                    it->pressure = pressure;

                    touchMoveEvent.id = it->id;
                    touchMoveEvent.position = it->position;
                    touchMoveEvent.delta = it->lastDelta;
                    touchMoveEvent.pressure = it->pressure;
                    SendEvent(touchMoveEvent);
                }

                break;
            }
        }

        // New touch
        if (!found)
        {
            // Use the first gap in current touches, or insert to the end if no gaps
            size_t insertIndex = touches.Size();
            unsigned newId = (unsigned)touches.Size();

            for (size_t i = 0; i < touches.Size(); ++i)
            {
                if (touches[i].id > i)
                {
                    insertIndex = i;
                    newId = i ? touches[i - 1].id + 1 : 0;
                    break;
                }
            }

            Touch newTouch;
            newTouch.id = newId;
            newTouch.internalId = internalId;
            newTouch.position = position;
            newTouch.pressure = pressure;
            touches.Insert(insertIndex, newTouch);

            touchBeginEvent.id = newTouch.id;
            touchBeginEvent.position = newTouch.position;
            touchBeginEvent.pressure = newTouch.pressure;
            SendEvent(touchBeginEvent);
        }
    }
    else
    {
        // End touch
        for (auto it = touches.Begin(); it != touches.End(); ++it)
        {
            if (it->internalId == internalId)
            {
                it->position = position;
                it->pressure = pressure;

                touchEndEvent.id = it->id;
                touchEndEvent.position = it->position;
                SendEvent(touchEndEvent);
                touches.Erase(it);
                break;
            }
        }
    }
}

void Input::OnGainFocus()
{
}

void Input::OnLoseFocus()
{
    mouseButtons = 0;
    mouseButtonsPressed = 0;
    mouseMove = IntVector2::ZERO;
    keyDown.Clear();
    keyPressed.Clear();
    rawKeyDown.Clear();
    rawKeyPress.Clear();
}

}
