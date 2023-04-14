// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/IntVector2.h"
#include "../Object/Object.h"

struct SDL_Window;

/// Button states for keys, mouse and controller.
enum ButtonState
{
    STATE_UP = 0,
    STATE_RELEASED,
    STATE_DOWN,
    STATE_PRESSED
};

/// %Input collection subsystem.
class Input : public Object
{
    OBJECT(Input);

public:
    /// Construct and register subsystem. Requires an OS-level window.
    Input(SDL_Window* window);
    /// Destruct.
    ~Input();

    /// Poll OS input events from the window.
    void Update();
   
    /// Return state of a key.
    ButtonState KeyState(unsigned keyCode) const;
    /// Return state of a mouse button.
    ButtonState MouseButtonState(unsigned num) const;
    /// Return whether key was pressed this frame.
    bool KeyPressed(unsigned keyCode) const { return KeyState(keyCode) == STATE_PRESSED; }
    /// Return whether key was released this frame.
    bool KeyReleased(unsigned keyCode) const { return KeyState(keyCode) == STATE_RELEASED; }
    /// Return whether key was pressed or held down this frame.
    bool KeyDown(unsigned keyCode) const { ButtonState state = KeyState(keyCode); return state >= STATE_DOWN; }
    /// Return whether mouse button was pressed this frame.
    bool MouseButtonPressed(unsigned num) const { return MouseButtonState(num) == STATE_PRESSED; }
    /// Return whether key was released this frame.
    bool MouseButtonReleased(unsigned num) const { return MouseButtonState(num) == STATE_RELEASED; }
    /// Return whether key was pressed or held down this frame.
    bool MouseButtonDown(unsigned num) const { ButtonState state = MouseButtonState(num); return state >= STATE_DOWN; }
    /// Return mouse movement since last frame.
    const IntVector2& MouseMove() const { return mouseMove; }
    /// Return mouse wheel scroll since last frame.
    const IntVector2& MouseWheel() const { return mouseWheel; }
    /// Return whether has input focus.
    bool HasFocus() const { return focus; }
    /// Return whether application exit was requested.
    bool ShouldExit() const { return shouldExit; }
    /// Return the OS-level window.
    SDL_Window* Window() const { return window; }

private:
    /// OS-level window.
    SDL_Window* window;
    /// Accumulated mouse movement.
    IntVector2 mouseMove;
    /// Accumulated mouse wheel movement.
    IntVector2 mouseWheel;
    /// Key states.
    std::map<unsigned, ButtonState> keyStates;
    /// Mouse button states.
    std::map<unsigned, ButtonState> mouseButtonStates;
    /// Input focus flag.
    bool focus;
    /// Exit request flag.
    bool shouldExit;
};
