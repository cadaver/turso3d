// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/IntVector2.h"
#include "../Object/Object.h"

namespace Turso3D
{

/// Left mouse button index.
static const unsigned MOUSEB_LEFT = 0;
/// Middle mouse button index.
static const unsigned MOUSEB_MIDDLE = 1;
/// Right mouse button index.
static const unsigned MOUSEB_RIGHT = 2;

/// Key press or release event.
class TURSO3D_API KeyEvent : public Event
{
public:
    /// Key code.
    unsigned keyCode;
    /// Raw key code.
    unsigned rawKeyCode;
    /// Pressed flag.
    bool pressed;
    /// Repeat flag.
    bool repeat;
};

/// Unicode character input event.
class TURSO3D_API CharInputEvent : public Event
{
public:
    /// Unicode codepoint.
    unsigned unicodeChar;
};

/// Mouse button press or release event.
class TURSO3D_API MouseButtonEvent : public Event
{
public:
    /// Button index.
    unsigned button;
    /// Bitmask of currently held down buttons.
    unsigned buttons;
    /// Pressed flag.
    bool pressed;
    /// Mouse position within window.
    IntVector2 position;
};

/// Mouse move event.
class TURSO3D_API MouseMoveEvent : public Event
{
public:
    /// Bitmask of currently held down buttons.
    unsigned buttons;
    /// Mouse position within window.
    IntVector2 position;
    /// Delta from last position.
    IntVector2 delta;
};

/// %Input subsystem for reading keyboard/mouse/etc. input. Updated from OS window messages by the Window class.
class TURSO3D_API Input : public Object
{
    OBJECT(Input);

public:
    /// Construct and register subsystem.
    Input();
    /// Destruct.
    ~Input();

    /// Poll the window (if any) for OS window messages and update input state.
    void Update();

    /// Return whether key is down by key code.
    bool IsKeyDown(unsigned keyCode) const;
    /// Return whether key is down by raw key code.
    bool IsKeyDownRaw(unsigned rawKeyCode) const;
    /// Return whether key was pressed on this frame by key code.
    bool IsKeyPressed(unsigned keyCode) const;
    /// Return whether key was pressed on this frame by raw key code.
    bool IsKeyPressedRaw(unsigned rawKeyCode) const;
    /// Return current mouse position.
    IntVector2 MousePosition() const { return mousePosition; }
    /// Return accumulated mouse movement since last frame.
    IntVector2 MouseMove() const { return mouseMove; }
    /// Return pressed down mouse buttons bitmask.
    unsigned MouseButtons() const { return mouseButtons; }
    /// Return whether a mouse button is down.
    bool IsMouseButtonDown(unsigned button) const;
    /// Return whether a mouse button was pressed on this frame.
    bool IsMouseButtonPressed(unsigned button) const;

    /// React to a key press or release. Called by window message handling.
    void OnKey(unsigned keyCode, unsigned rawKeyCode, bool pressed);
    /// React to char input. Called by window message handling.
    void OnChar(unsigned unicodeChar);
    /// React to a mouse move. Called by window message handling.
    void OnMouseMove(const IntVector2& position);
    /// React to a mouse button. Called by window message handling.
    void OnMouseButton(unsigned button, bool pressed);
    /// React to gaining focus. Called by window message handling.
    void OnGainFocus();
    /// React to losing focus. Called by window message handling.
    void OnLoseFocus();

    /// Key press/release event.
    KeyEvent keyEvent;
    /// Unicode char input event.
    CharInputEvent charInputEvent;
    /// Mouse button press/release event.
    MouseButtonEvent mouseButtonEvent;
    /// Mouse move event.
    MouseMoveEvent mouseMoveEvent;

private:
    /// Key code held down status.
    HashMap<unsigned, bool> keyDown;
    /// Key code pressed status.
    HashMap<unsigned, bool> keyPressed;
    /// Raw key code held down status.
    HashMap<unsigned, bool> rawKeyDown;
    /// Raw key code pressed status.
    HashMap<unsigned, bool> rawKeyPressed;
    /// Current mouse position.
    IntVector2 mousePosition;
    /// Accumulated mouse move since last frame.
    IntVector2 mouseMove;
    /// Mouse buttons bitmask.
    unsigned mouseButtons;
    /// Mouse buttons pressed bitmask.
    unsigned mouseButtonsPressed;
    /// Discard next mouse move flag.
    bool discardMouseMove;
};

}
