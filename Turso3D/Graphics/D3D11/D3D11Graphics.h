// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Math/Color.h"
#include "../../Math/IntVector2.h"
#include "../../Object/Object.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

struct GraphicsImpl;
class Window;
class WindowResizeEvent;

/// 3D graphics rendering context. Manages the rendering window and GPU objects.
class Graphics : public Object
{
    OBJECT(Graphics);

public:
    /// Construct and register subsystem. The graphics mode is not set & window is not opened yet.
    Graphics();
    /// Destruct. Clean up the window, rendering context and GPU objects.
    ~Graphics();

    /// Set graphics mode. Create the window and rendering context if not created yet. Return true on success.
    bool SetMode(int width, int height, bool resizable);
    /// Return backbuffer width, or 0 if not initialized.
    int Width() const { return backBufferSize.x; }
    /// Return backbuffer height, or 0 if not initialized.
    int Height() const { return backBufferSize.y; }
    /// Close the window and destroy the rendering context and GPU objects.
    void Close();
    /// Clear the current rendertarget.
    void Clear(unsigned clearFlags, const Color& clearColor = Color::BLACK, float clearDepth = 1.0f, unsigned char clearStencil = 0);
    /// Present the contents of the backbuffer.
    void Present();

    /// Return whether has the rendering window and context.
    bool IsInitialized() const;
    /// Return the rendering window.
    Window* RenderWindow() const;

private:
    /// Create the D3D11 device and swap chain. Requires an open window. Return true on success.
    bool CreateDevice();
    /// Update buffers to match window size and create default views for the backbuffer and depth buffer. Return true on success.
    bool UpdateBuffersAndViews();
    /// Resize the backbuffer when window size changes.
    void HandleResize(WindowResizeEvent& event);

    /// Implementation for holding OS-specific API objects.
    AutoPtr<GraphicsImpl> impl;
    /// OS-level rendering window.
    AutoPtr<Window> window;
    /// Current size of the backbuffer.
    IntVector2 backBufferSize;
};

}