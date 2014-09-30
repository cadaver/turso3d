// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Math/Color.h"
#include "../../Math/IntVector2.h"
#include "../../Object/Object.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

struct GraphicsImpl;
class ConstantBuffer;
class GPUObject;
class IndexBuffer;
class ShaderVariation;
class VertexBuffer;
class Window;
class WindowResizeEvent;

typedef Pair<unsigned long long, unsigned> InputLayoutDesc;
typedef HashMap<InputLayoutDesc, void*> InputLayoutMap;

/// 3D graphics rendering context. Manages the rendering window and GPU objects.
class TURSO3D_API Graphics : public Object
{
    OBJECT(Graphics);

public:
    /// Construct and register subsystem. The graphics mode is not set & window is not opened yet.
    Graphics();
    /// Destruct. Clean up the window, rendering context and GPU objects.
    ~Graphics();

    /// Set graphics mode. Create the window and rendering context if not created yet. Return true on success.
    bool SetMode(int width, int height, bool fullscreen, bool resizable);
    /// Switch between fullscreen/windowed while retaining previous resolution. Return true on success.
    bool SwitchFullscreen();
    /// Close the window and destroy the rendering context and GPU objects.
    void Close();
    /// Clear the current rendertarget.
    void Clear(unsigned clearFlags, const Color& clearColor = Color::BLACK, float clearDepth = 1.0f, unsigned char clearStencil = 0);
    /// Present the contents of the backbuffer.
    void Present();
    /// Bind a vertex buffer.
    void SetVertexBuffer(size_t index, VertexBuffer* buffer);
    /// Bind an index buffer.
    void SetIndexBuffer(IndexBuffer* buffer);
    /// Bind a constant buffer.
    void SetConstantBuffer(ShaderStage stage, size_t index, ConstantBuffer* buffer);
    /// Clear all bound vertex buffers.
    void ResetVertexBuffers();
    /// Clear all bound constant buffers.
    void ResetConstantBuffers();
    /// Bind vertex and pixel shaders.
    void SetShaders(ShaderVariation* vs, ShaderVariation* ps);
    /// Draw non-indexed geometry.
    void Draw(PrimitiveType type, size_t vertexStart, size_t vertexCount);
    /// Draw indexed geometry.
    void DrawIndexed(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart = 0);

    /// Return whether has the rendering window and context.
    bool IsInitialized() const;
    /// Return backbuffer width, or 0 if not initialized.
    int Width() const { return backbufferSize.x; }
    /// Return backbuffer height, or 0 if not initialized.
    int Height() const { return backbufferSize.y; }
    /// Return whether is using fullscreen mode
    bool IsFullscreen() const { return fullscreen; }
    /// Return the rendering window.
    Window* RenderWindow() const;
    /// Return the D3D11 device.
    void* Device() const;
    /// Return the D3D11 immediate device context.
    void* DeviceContext() const;
    /// Return currently bound vertex buffer by index.
    VertexBuffer* CurrentVertexBuffer(size_t index) const;
    /// Return currently bound index buffer.
    IndexBuffer* CurrentIndexBuffer() const { return indexBuffer; }
    /// Return currently bound constant buffer by shader stage and index.
    ConstantBuffer* CurrentConstantBuffer(ShaderStage stage, size_t index) const;
    /// Return currently bound vertex shader.
    ShaderVariation* CurrentVertexShader() const { return vertexShader; }
    /// Return currently bound pixel shader.
    ShaderVariation* CurrentPixelShader() const { return pixelShader; }

    /// Register a GPU object to keep track of.
    void AddGPUObject(GPUObject* object);
    /// Remove a GPU object.
    void RemoveGPUObject(GPUObject* object);

private:
    /// Create the D3D11 device and swap chain. Requires an open window. Return true on success.
    bool CreateDevice();
    /// Update swap chain state for a new mode and create views for the backbuffer & default depth buffer.
    bool UpdateSwapChain(int width, int height, bool fullscreen);
    /// Resize the backbuffer when window size changes.
    void HandleResize(WindowResizeEvent& event);
    /// Set topology, and find or create an input layout for the currently set vertex buffers and vertex shader.
    void PrepareDraw(PrimitiveType type);
    /// Reset internally tracked state.
    void ResetState();

    /// Implementation for holding OS-specific API objects.
    AutoPtr<GraphicsImpl> impl;
    /// OS-level rendering window.
    AutoPtr<Window> window;
    /// Current size of the backbuffer.
    IntVector2 backbufferSize;
    /// GPU objects.
    Vector<GPUObject*> gpuObjects;
    /// Input layouts.
    InputLayoutMap inputLayouts;
    /// Bound vertex buffers.
    VertexBuffer* vertexBuffers[MAX_VERTEX_STREAMS];
    /// Bound index buffer.
    IndexBuffer* indexBuffer;
    /// Bound constant buffers by shader stage.
    ConstantBuffer* constantBuffers[MAX_SHADER_STAGES][MAX_CONSTANT_BUFFERS];
    /// Bound vertex shader.
    ShaderVariation* vertexShader;
    /// Bound pixel shader.
    ShaderVariation* pixelShader;
    /// Current primitive type.
    PrimitiveType primitiveType;
    /// Current input layout: vertex buffers' element mask and vertex shader's element mask combined.
    InputLayoutDesc inputLayout;
    /// Fullscreen flag.
    bool fullscreen;
    /// Resize handling flag to prevent recursion.
    bool inResize;
    /// Input layout dirty flag.
    bool inputLayoutDirty;
};

}