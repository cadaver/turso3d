// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Turso3DConfig.h"

#ifdef TURSO3D_OPENGL

#include "../../Base/Ptr.h"

namespace Turso3D
{

class Window;

/// OpenGL context associated with a window, Win32 implementation.
class TURSO3D_API GLContext
{
public:
    /// Construct. Associate with a window, but do not create the context yet.
    GLContext(Window* window);
    /// Destruct. Destroy the context if created.
    ~GLContext();

    /// Create context and initialize extensions. Return true on success. The pixel format can only be chosen once, so a context can not be created more than once to the same window.
    bool Create(int multisample);
    /// Present the backbuffer.
    void Present();
    /// Set vsync on/off.
    void SetVSync(bool enable);

    /// Return whether is initialized with a valid context.
    bool IsInitialized() const { return contextHandle != nullptr; }

private:
    /// Destroy the context.
    void Release();

    /// Associated window.
    WeakPtr<Window> window;
    /// Window device context handle.
    void* dcHandle;
    /// OpenGL context handle.
    void* contextHandle;
};

#endif

}
