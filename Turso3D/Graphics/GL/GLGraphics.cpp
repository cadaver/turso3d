// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Window/GLContext.h"
#include "../../Window/Window.h"
#include "../GPUObject.h"
#include "../Shader.h"
#include "GLBlendState.h"
#include "GLDepthState.h"
#include "GLGraphics.h"
#include "GLConstantBuffer.h"
#include "GLIndexBuffer.h"
#include "GLRasterizerState.h"
#include "GLShaderProgram.h"
#include "GLShaderVariation.h"
#include "GLTexture.h"
#include "GLVertexBuffer.h"

#include <cstdlib>
#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

Graphics::Graphics() :
    backbufferSize(IntVector2::ZERO),
    renderTargetSize(IntVector2::ZERO),
    fullscreen(false),
    vsync(false),
    inResize(false)
{
    RegisterSubsystem(this);
    window = new Window();
    SubscribeToEvent(window->resizeEvent, &Graphics::HandleResize);
    ResetState();
}

Graphics::~Graphics()
{
    Close();
    RemoveSubsystem(this);
}

bool Graphics::SetMode(int width, int height, bool fullscreen, bool resizable)
{
    // Setting window size only required if window not open yet, otherwise the swapchain takes care of resizing
    if (!window->SetSize(width, height, resizable))
        return false;

    if (!context)
    {
        context = new GLContext(window);
        if (!context->Create())
        {
            context.Reset();
            return false;
        }
    }

    /// \todo Set screen mode
}

bool Graphics::SetFullscreen(bool enable)
{
    if (!IsInitialized())
        return false;
    else
        return SetMode(backbufferSize.x, backbufferSize.y, enable, window->IsResizable());
}

void Graphics::SetVSync(bool enable)
{
    vsync = enable;
    /// \todo Set vsync through the GL context
}

void Graphics::Close()
{
    shaderPrograms.Clear();

    // Release all GPU objects
    for (auto it = gpuObjects.Begin(); it != gpuObjects.End(); ++it)
    {
        GPUObject* object = *it;
        object->Release();
    }

    context.Reset();
    window->Close();
    backbufferSize = IntVector2::ZERO;
    ResetState();
}

void Graphics::Present()
{
    context->Present();
}

void Graphics::SetRenderTarget(Texture* renderTarget_, Texture* depthStencil_)
{
    renderTargetVector.Resize(1);
    renderTargetVector[0] = renderTarget_;
    SetRenderTargets(renderTargetVector, depthStencil_);
}

void Graphics::SetRenderTargets(const Vector<Texture*>& renderTargets_, Texture* depthStencil_)
{
    if (renderTargets_.IsEmpty())
        return;

    for (size_t i = 0; i < MAX_RENDERTARGETS && i < renderTargets_.Size(); ++i)
    {
        renderTargets[i] = renderTargets_[i];
    }

    for (size_t i = renderTargets_.Size(); i < MAX_RENDERTARGETS; ++i)
    {
        renderTargets[i] = nullptr;
    }

    depthStencil = depthStencil_;

    if (renderTargets[0])
        renderTargetSize = IntVector2(renderTargets[0]->Width(), renderTargets[0]->Height());
    else
        renderTargetSize = backbufferSize;

    /// \todo Manage OpenGL rendertarget changes
}

void Graphics::SetViewport(const IntRect& viewport_)
{
    /// \todo Implement a member function in IntRect for clipping
    viewport.left = Clamp(viewport_.left, 0, renderTargetSize.x - 1);
    viewport.top = Clamp(viewport_.top, 0, renderTargetSize.y - 1);
    viewport.right = Clamp(viewport_.right, viewport.left + 1, renderTargetSize.x);
    viewport.bottom = Clamp(viewport_.bottom, viewport.top + 1, renderTargetSize.y);

    /// \todo Set OpenGL viewport
}

void Graphics::SetVertexBuffer(size_t index, VertexBuffer* buffer)
{
    if (index < MAX_VERTEX_STREAMS && vertexBuffers[index] != buffer)
    {
        vertexBuffers[index] = buffer;
        inputLayoutDirty = true;
        /// \todo Manage OpenGL vertex buffer changes
    }
}

void Graphics::SetConstantBuffer(ShaderStage stage, size_t index, ConstantBuffer* buffer)
{
    if (stage < MAX_SHADER_STAGES &&index < MAX_CONSTANT_BUFFERS && constantBuffers[stage][index] != buffer)
    {
        constantBuffers[stage][index] = buffer;
        /// \todo Manage OpenGL constant buffer changes
    }
}

void Graphics::SetTexture(size_t index, Texture* texture)
{
    if (index < MAX_TEXTURE_UNITS)
    {
        textures[index] = texture;
        /// \odo Manage OpenGL texture change
    }
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    if (indexBuffer != buffer)
    {
        indexBuffer = buffer;
        /// \todo Manage OpenGL index buffer change
    }
}

void Graphics::SetShaders(ShaderVariation* vs, ShaderVariation* ps)
{
    if (vs == vertexShader && ps == pixelShader)
        return;

    if (vs != vertexShader)
    {
        if (vs && vs->Stage() == SHADER_VS)
        {
            if (!vs->IsCompiled())
                vs->Compile();
        }

        vertexShader = vs;
    }

    if (ps != pixelShader)
    {
        if (ps && ps->Stage() == SHADER_PS)
        {
            if (!ps->IsCompiled())
                ps->Compile();
        }

        pixelShader = ps;
    }

    if (vertexShader && pixelShader && vertexShader->ShaderObject() && pixelShader->ShaderObject())
    {
        // Check if program already exists, if not, link now
        auto key = MakePair(vertexShader, pixelShader);
        auto it = shaderPrograms.Find(key);
        if (it != shaderPrograms.End())
            glUseProgram(it->second->ProgramObject());
        else
        {
            ShaderProgram* newProgram = new ShaderProgram(vertexShader, pixelShader);
            shaderPrograms[key] = newProgram;
            if (newProgram->Link())
                glUseProgram(newProgram->ProgramObject());
        }
    }
    else
        glUseProgram(0);
}

void Graphics::SetBlendState(BlendState* state)
{
    if (state != blendState)
    {
        /// \todo Apply to OpenGL
        blendState = state;
    }
}

void Graphics::SetDepthState(DepthState* state, unsigned stencilRef_)
{
    if (state != depthState || stencilRef_ != stencilRef)
    {
        /// \todo Apply to OpenGL
        depthState = state;
    }
}

void Graphics::SetRasterizerState(RasterizerState* state)
{
    if (state != rasterizerState)
    {
        /// \todo Apply to OpenGL
        rasterizerState = state;
    }
}

void Graphics::SetScissorRect(const IntRect& scissorRect_)
{
    if (scissorRect_ != scissorRect)
    {
        /// \todo Implement a member function in IntRect for clipping
        scissorRect.left = Clamp(scissorRect_.left, 0, renderTargetSize.x - 1);
        scissorRect.top = Clamp(scissorRect_.top, 0, renderTargetSize.y - 1);
        scissorRect.right = Clamp(scissorRect_.right, scissorRect.left + 1, renderTargetSize.x);
        scissorRect.bottom = Clamp(scissorRect_.bottom, scissorRect.top + 1, renderTargetSize.y);

        /// \todo Apply to OpenGL
    }
}

void Graphics::ResetRenderTargets()
{
    SetRenderTarget(nullptr, nullptr);
}

void Graphics::ResetVertexBuffers()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        SetVertexBuffer(i, nullptr);
}

void Graphics::ResetConstantBuffers()
{
    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        for (size_t j = 0; i < MAX_CONSTANT_BUFFERS; ++j)
            SetConstantBuffer((ShaderStage)i, j, nullptr);
    }
}

void Graphics::Clear(unsigned clearFlags, const Color& clearColor, float clearDepth, unsigned char clearStencil)
{
    unsigned glFlags = 0;
    if (clearFlags & CLEAR_COLOR)
    {
        glFlags |= GL_COLOR_BUFFER_BIT;
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    }
    if (clearFlags & CLEAR_DEPTH)
    {
        glFlags |= GL_DEPTH_BUFFER_BIT;
        glClearDepth(clearDepth);
    }
    if (clearFlags & CLEAR_STENCIL)
    {
        glFlags |= GL_STENCIL_BUFFER_BIT;
        glClearStencil(clearStencil);
    }
    glClear(glFlags);
}

void Graphics::Draw(PrimitiveType type, size_t vertexStart, size_t vertexCount)
{
    PrepareDraw(type);
    /// \todo Implement
}

void Graphics::Draw(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart)
{
    PrepareDraw(type);
    /// \todo Implement
}

void Graphics::DrawInstanced(PrimitiveType type, size_t vertexStart, size_t vertexCount, size_t instanceStart, size_t instanceCount)
{
    PrepareDraw(type);
    /// \todo Implement
}

void Graphics::DrawInstanced(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart, size_t instanceStart, size_t instanceCount)
{
    PrepareDraw(type);
    /// \todo Implement
}

bool Graphics::IsInitialized() const
{
    return window->IsOpen() && context;
}

bool Graphics::IsResizable() const
{
    return window->IsResizable();
}

Window* Graphics::RenderWindow() const
{
    return window;
}

Texture* Graphics::RenderTarget(size_t index) const
{
    return index < MAX_RENDERTARGETS ? renderTargets[index] : nullptr;
}

VertexBuffer* Graphics::GetVertexBuffer(size_t index) const
{
    return index < MAX_VERTEX_STREAMS ? vertexBuffers[index] : nullptr;
}

ConstantBuffer* Graphics::GetConstantBuffer(ShaderStage stage, size_t index) const
{
    return (stage < MAX_SHADER_STAGES && index < MAX_CONSTANT_BUFFERS) ? constantBuffers[stage][index] : nullptr;
}

Texture* Graphics::GetTexture(size_t index) const
{
    return (index < MAX_TEXTURE_UNITS) ? textures[index] : nullptr;
}

void Graphics::AddGPUObject(GPUObject* object)
{
    if (object)
        gpuObjects.Push(object);
}

void Graphics::RemoveGPUObject(GPUObject* object)
{
    /// \todo Requires a linear search, needs to be profiled whether becomes a problem with a large number of objects
    gpuObjects.Remove(object);
}

void Graphics::CleanupShaderPrograms(ShaderVariation* shader)
{
    if (!shader)
        return;

    if (shader->Stage() == SHADER_VS)
    {
        for (auto it = shaderPrograms.Begin(); it != shaderPrograms.End();)
        {
            if (it->first.first == shader)
                it = shaderPrograms.Erase(it);
            else
                ++it;
        }
    }
    else
    {
        for (auto it = shaderPrograms.Begin(); it != shaderPrograms.End();)
        {
            if (it->first.second == shader)
                it = shaderPrograms.Erase(it);
            else
                ++it;
        }
    }
}

void Graphics::HandleResize(WindowResizeEvent& /*event*/)
{
    // Handle windowed mode resize
    /// \todo Implement
}

void Graphics::PrepareDraw(PrimitiveType type)
{
    /// \todo Implement
}

void Graphics::ResetState()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        vertexBuffers[i] = nullptr;

    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        for (size_t j = 0; j < MAX_CONSTANT_BUFFERS; ++j)
            constantBuffers[i][j] = nullptr;
    }

    for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        textures[i] = nullptr;
    }

    indexBuffer = nullptr;
    vertexShader = nullptr;
    pixelShader = nullptr;
    blendState = nullptr;
    depthState = nullptr;
    rasterizerState = nullptr;
    inputLayout.first = 0;
    inputLayout.second = 0;
    inputLayoutDirty = false;
    primitiveType = MAX_PRIMITIVE_TYPES;
    scissorRect = IntRect();
    stencilRef = 0;
}

void RegisterGraphicsLibrary()
{
    Shader::RegisterObject();
    Texture::RegisterObject();
}

}
