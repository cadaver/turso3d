// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "../Math/Color.h"
#include "../Math/IntRect.h"
#include "../Math/Matrix3x4.h"
#include "../Object/Object.h"
#include "../Time/Timer.h"
#include "GraphicsDefs.h"

class FrameBuffer;
class IndexBuffer;
class ShaderProgram;
class Texture;
class UniformBuffer;
class VertexBuffer;
struct SDL_Window;

/// Occlusion query result.
struct OcclusionQueryResult
{
    /// Query ID.
    unsigned id;
    /// Associated object.
    void* object;
    /// Visibility result.
    bool visible;
};

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

    /// Bind a framebuffer for rendering. Null buffer parameter to unbind and return to backbuffer rendering. Provided for convenience.
    void SetFrameBuffer(FrameBuffer* buffer);
    /// Set the viewport rectangle.
    void SetViewport(const IntRect& viewRect);
    /// Bind a shader program for use. Return pointer on success or null otherwise. Low performance, provided for convenience.
    ShaderProgram* SetProgram(const std::string& shaderName, const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
    /// Create a shader program, but do not bind immediately. Return pointer on success or null otherwise.
    ShaderProgram* CreateProgram(const std::string& shaderName, const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
    /// Set float preset uniform.
    void SetUniform(ShaderProgram* program, PresetUniform uniform, float value);
    /// Set a Vector2 preset uniform.
    void SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector2& value);
    /// Set a Vector3 preset uniform.
    void SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector3& value);
    /// Set a Vector4 preset uniform.
    void SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector4& value);
    /// Set a Matrix3x4 preset uniform.
    void SetUniform(ShaderProgram* program, PresetUniform uniform, const Matrix3x4& value);
    /// Set a Matrix4 preset uniform.
    void SetUniform(ShaderProgram* program, PresetUniform uniform, const Matrix4& value);
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
    /// Bind a uniform buffer for use in slot index. Null buffer parameter to unbind. Provided for convenience.
    void SetUniformBuffer(size_t index, UniformBuffer* buffer);
    /// Bind a texture for use in texture unit. Null texture parameter to unbind.  Provided for convenience.
    void SetTexture(size_t index, Texture* texture);
    /// Bind a vertex buffer for use with the specified shader program's attribute bindings. Provided for convenience.
    void SetVertexBuffer(VertexBuffer* buffer, ShaderProgram* program);
    /// Bind an index buffer for use. Provided for convenience.
    void SetIndexBuffer(IndexBuffer* buffer);
    /// Set basic renderstates.
    void SetRenderState(BlendMode blendMode, CullMode cullMode = CULL_BACK, CompareMode depthTest = CMP_LESS, bool colorWrite = true, bool depthWrite = true);
    /// Set depth bias.
    void SetDepthBias(float constantBias = 0.0f, float slopeScaleBias = 0.0f);
    /// Clear the current framebuffer.
    void Clear(bool clearColor = true, bool clearDepth = true, const IntRect& clearRect = IntRect::ZERO, const Color& backgroundColor = Color::BLACK);
    /// Blit from one framebuffer to another. The destination framebuffer will be left bound for rendering.
    void Blit(FrameBuffer* dest, const IntRect& destRect, FrameBuffer* src, const IntRect& srcRect, bool blitColor, bool blitDepth, TextureFilterMode filter);
    /// Draw non-indexed geometry with the currently bound vertex buffer.
    void Draw(PrimitiveType type, size_t drawStart, size_t drawCount);
   /// Draw indexed geometry with the currently bound vertex and index buffer.
    void DrawIndexed(PrimitiveType type, size_t drawStart, size_t drawCount);
    /// Draw instanced non-indexed geometry with the currently bound vertex and index buffer, and the specified instance data vertex buffer.
    void DrawInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, VertexBuffer* instanceVertexBuffer, size_t instanceStart, size_t instanceCount);
    /// Draw instanced indexed geometry with the currently bound vertex and index buffer, and the specified instance data vertex buffer.
    void DrawIndexedInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, VertexBuffer* instanceVertexBuffer, size_t instanceStart, size_t instanceCount);
    /// Draw a quad with current renderstate. The quad vertex buffer is left bound.
    void DrawQuad();

    /// Begin an occlusion query and associate an object with it for checking results. Return the query ID.
    unsigned BeginOcclusionQuery(void* object);
    /// End an occlusion query.
    void EndOcclusionQuery();
    /// Free an occlusion query when its associated object is destroyed early.
    void FreeOcclusionQuery(unsigned id);
    /// Check for and return arrived query results. These are only retained for one frame.
    void CheckOcclusionQueryResults(std::vector<OcclusionQueryResult>& result);
    /// Return number of pending occlusion queries.
    size_t PendingOcclusionQueries() const { return pendingQueries.size(); }

    /// Return whether is initialized.
    bool IsInitialized() const { return context != nullptr; }
    /// Return whether has instancing support.
    bool HasInstancing() const { return hasInstancing; }
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
    /// Return last frame interval in seconds.
    float LastFrameTime() const { return lastFrameTime; }
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
    /// Instancing support flag.
    bool hasInstancing;
    /// Whether instance vertex elements are enabled.
    bool instancingEnabled;
    /// Pending occlusion queries.
    std::vector<std::pair<unsigned, void*> > pendingQueries;
    /// Free occlusion queries.
    std::vector<unsigned> freeQueries;
    /// Frame timer.
    HiresTimer frameTimer;
    /// Last frame interval in seconds.
    float lastFrameTime;
};

/// Register Graphics related object factories and attributes.
void RegisterGraphicsLibrary();
