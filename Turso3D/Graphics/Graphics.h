// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "../Math/Color.h"
#include "../Math/IntRect.h"
#include "../Math/Matrix3x4.h"
#include "../Object/Object.h"
#include "GraphicsDefs.h"

class FrameBuffer;
class ShaderProgram;
class VertexBuffer;
struct SDL_Window;

/// %Graphics rendering context and application window.
class Graphics : public Object
{
    OBJECT(Graphics);

public:
    /// Create window with initial size and register subsystem and object. Rendering context is not created yet.
    Graphics(const char* windowTitle, const IntVector2& windowSize);
    /// Destruct. Closes the application window.
    ~Graphics();

    /// Initialize rendering context. Return true on success.
    bool Initialize();
    /// Set new window size.
    void Resize(const IntVector2& size);
    /// Set fullscreen mode.
    void SetFullscreen(bool enable);
    /// Set vertical sync on/off.
    void SetVSync(bool enable);
    /// Present the contents of the backbuffer.
    void Present();

    /// Set the viewport rectangle.
    void SetViewport(const IntRect& viewRect);
    /// Set basic renderstates.
    void SetRenderState(BlendMode blendMode, CullMode cullMode = CULL_BACK, CompareMode depthTest = CMP_LESS, bool colorWrite = true, bool depthWrite = true);
    /// Set depth bias.
    void SetDepthBias(float constantBias = 0.0f, float slopeScaleBias = 0.0f);
    /// Bind a shader program for use. Provided for convenience. Return pointer on success or null otherwise.
    ShaderProgram* SetProgram(const std::string& shaderName, const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
    /// Set float uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, float value);
    /// Set a Vector2 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector2& value);
    /// Set a Vector3 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector3& value);
    /// Set a Vector4 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector4& value);
    /// Set a Matrix3x4 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Matrix3x4& value);
    /// Set a Matrix4 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Matrix4& value);
    /// Clear the current framebuffer.
    void Clear(bool clearColor = true, bool clearDepth = true, const IntRect& clearRect = IntRect::ZERO, const Color& backgroundColor = Color::BLACK);
    /// Draw a quad with current renderstate.
    void DrawQuad();

    /// Return whether is initialized.
    bool IsInitialized() const { return context != nullptr; }
    /// Return current window size.
    IntVector2 Size() const;
    /// Return current window width.
    int Width() const { return Size().x; }
    /// Return current window height.
    int Height() const { return Size().y; }
    /// Return window render size, which can be different if the OS is doing resolution scaling.
    IntVector2 RenderSize() const;
    /// Return window render width.
    int RenderWidth() const { return RenderSize().x; }
    /// Return window render height.
    int RenderHeight() const { return RenderSize().y; }
    /// Return whether is fullscreen.
    bool IsFullscreen() const;
    /// Return whether is using vertical sync.
    bool VSync() const { return vsync; }
    /// Return the OS-level window.
    SDL_Window* Window() const { return window; }

private:
    /// Set up the vertex buffer for quad rendering.
    void DefineQuadVertexBuffer();

    /// OS-level rendering window.
    SDL_Window* window;
    /// OpenGL context.
    void* context;
    /// Quad vertex buffer.
    AutoPtr<VertexBuffer> quadVertexBuffer;
    /// Last blend mode.
    BlendMode lastBlendMode;
    /// Last cull mode.
    CullMode lastCullMode;
    /// Last depth test.
    CompareMode lastDepthTest;
    /// Last color write.
    bool lastColorWrite;
    /// Last depth write.
    bool lastDepthWrite;
    /// Last depth bias enabled.
    bool lastDepthBias;
    /// Vertical sync flag.
    bool vsync;
};

/// Register Graphics related object factories and attributes.
void RegisterGraphicsLibrary();
