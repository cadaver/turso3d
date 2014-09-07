// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Object/Object.h"

namespace Turso3D
{

/// %Window resized event.
class TURSO3D_API WindowResizeEvent : public Event
{
public:
    /// New window width.
    int width;
    /// New window height.
    int height;
};

/// An operating system window, Win32 implementation.
class TURSO3D_API Window : public Object
{
    OBJECT(Window);

public:
    /// Construct and register subsystem. The window is not yet opened.
    Window();
    /// Destruct. Close window if open.
    virtual  ~Window();

    /// Set window title.
    void SetTitle(const String& newTitle);
    /// Set window size. Opens the window if not opened yet. Return true on success.
    bool SetSize(int width, int height, bool resizable);
    /// Close the window.
    void Close();
    /// Minimize the window.
    void Minimize();
    /// Maximize the window.
    void Maximize();
    /// Restore window size.
    void Restore();
    /// Pump window messages from the operating system.
    void PumpMessages();

    /// Return window title.
    const String& Title() const { return title; }
    /// Return window client area width.
    int Width() const;
    /// Return window client area height.
    int Height() const;
    /// Return whether window is open.
    bool IsOpen() const { return handle != 0; }
    /// Return whether is resizable.
    bool IsResizable() const;
    /// Return whether is currently minimized.
    bool IsMinimized() const;
    /// Return whether has input focus.
    bool HasFocus() const;
    /// Return window handle. Can be cast to a HWND.
    void* Handle() const { return handle; }

    /// Handle the window being closed. Called internally.
    void OnClose();
    /// Handle a size change, minimization or restore. Called internally.
    void OnSizeChange(unsigned mode);

    /// Event for window close being requested.
    Event closeRequestEvent;
    /// Event for window gaining focus.
    Event gainFocusEvent;
    /// Event for window losing focus.
    Event loseFocusEvent;
    /// Event for window being minimized.
    Event minimizeEvent;
    /// Event for window being restored after being minimized.
    Event restoreEvent;
    /// Event for window resized.
    WindowResizeEvent resizeEvent;

private:
    /// Window handle.
    void* handle;
    /// Window title.
    String title;
    /// Previous minimization state.
    bool wasMinimized;
};

}
