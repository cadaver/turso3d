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

static const unsigned elementGLType[] =
{
    GL_INT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_BYTE,
    GL_FLOAT,
    GL_FLOAT
};

static const unsigned elementGLComponents[] =
{
    1,
    1,
    2,
    3,
    4,
    4,
    12,
    16
};

static const unsigned glPrimitiveType[] = 
{
    0,
    GL_POINTS,
    GL_LINES,
    GL_LINE_STRIP,
    GL_TRIANGLES,
    GL_TRIANGLE_STRIP
};

Graphics::Graphics() :
    backbufferSize(IntVector2::ZERO),
    renderTargetSize(IntVector2::ZERO),
    attributeBySemantic(MAX_ELEMENT_SEMANTICS),
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

        context->SetVSync(vsync);

        // Query OpenGL capabilities
        int numBlocks;
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS, &numBlocks);
        vsConstantBuffers = numBlocks;
        glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, &numBlocks);
        psConstantBuffers = numBlocks;

        // Create and bind a vertex array object that will stay in use throughout
        /// \todo Investigate performance gain of using multiple VAO's
        unsigned vertexArrayObject;
        glGenVertexArrays(1, &vertexArrayObject);
        glBindVertexArray(vertexArrayObject);

        // Set up texture data read/write alignment. It is important that this is done before uploading any texture data
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }

    /// \todo Set fullscreen screen mode
    return true;
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
    if (context)
        context->SetVSync(enable);
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

    glViewport(viewport.left, viewport.top, viewport.Width(), viewport.Height());
}

void Graphics::SetVertexBuffer(size_t index, VertexBuffer* buffer)
{
    if (index < MAX_VERTEX_STREAMS && buffer != vertexBuffers[index])
    {
        vertexBuffers[index] = buffer;
        vertexBuffersDirty = true;
    }
}

void Graphics::SetConstantBuffer(ShaderStage stage, size_t index, ConstantBuffer* buffer)
{
    if (stage < MAX_SHADER_STAGES && index < MAX_CONSTANT_BUFFERS && buffer != constantBuffers[stage][index])
    {
        constantBuffers[stage][index] = buffer;
        unsigned bufferObject = buffer ? buffer->GLBuffer() : 0;

        switch (stage)
        {
        case SHADER_VS:
            if (index < vsConstantBuffers)
                glBindBufferBase(GL_UNIFORM_BUFFER, (unsigned)index, bufferObject);
            break;

        case SHADER_PS:
            if (index < psConstantBuffers)
                glBindBufferBase(GL_UNIFORM_BUFFER, (unsigned)(index + vsConstantBuffers), bufferObject);
            break;

        default:
            break;
        }
    }
}

void Graphics::SetTexture(size_t index, Texture* texture)
{
    if (index < MAX_TEXTURE_UNITS && texture != textures[index])
    {
        textures[index] = texture;
        
        if (index != activeTexture)
        {
            glActiveTexture(GL_TEXTURE0 + (unsigned)index);
            activeTexture = index;
        }
        if (texture)
        {
            unsigned target = texture->GLTarget();
            if (target != textureTargets[index])
            {
                if (textureTargets[index])
                    glDisable(textureTargets[index]);
                glEnable(target);
                textureTargets[index] = target;
            }
            glBindTexture(target, texture->GLTexture());
        }
        else if (textureTargets[index])
            glBindTexture(textureTargets[index], 0);
    }
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    if (indexBuffer != buffer)
    {
        indexBuffer = buffer;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer ? buffer->GLBuffer() : 0);
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

    if (vertexShader && pixelShader && vertexShader->GLShader() && pixelShader->GLShader())
    {
        // Check if program already exists, if not, link now
        auto key = MakePair(vertexShader, pixelShader);
        auto it = shaderPrograms.Find(key);
        if (it != shaderPrograms.End())
        {
            shaderProgram = it->second;
            glUseProgram(it->second->GLProgram());
        }
        else
        {
            ShaderProgram* newProgram = new ShaderProgram(vertexShader, pixelShader);
            shaderPrograms[key] = newProgram;
            // Note: if the linking is successful, glUseProgram() will have been called
            if (newProgram->Link())
                shaderProgram = newProgram;
            else
            {
                shaderProgram = nullptr;
                glUseProgram(0);
            }
        }
    }
    else
    {
        shaderProgram = nullptr;
        glUseProgram(0);
    }

    vertexAttributesDirty = true;
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
    PrepareDraw();
    glDrawArrays(glPrimitiveType[type], (unsigned)vertexStart, (unsigned)vertexCount);
}

void Graphics::Draw(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart)
{
    if (!indexBuffer)
        return;
    
    size_t indexSize = indexBuffer->IndexSize();

    PrepareDraw();
    if (!vertexStart)
    {
        glDrawElements(glPrimitiveType[type], (unsigned)indexCount, indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT :
            GL_UNSIGNED_INT, (const void*)(indexStart * indexSize));
    }
    else
    {
        glDrawElementsBaseVertex(glPrimitiveType[type], (unsigned)indexCount, indexSize == sizeof(unsigned short) ?
            GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(indexStart * indexSize), (unsigned)vertexStart);
    }

}

void Graphics::DrawInstanced(PrimitiveType type, size_t vertexStart, size_t vertexCount, size_t instanceStart, size_t
    instanceCount)
{
    PrepareDraw(true, instanceStart);
    glDrawArraysInstanced(glPrimitiveType[type], (unsigned)vertexStart, (unsigned)vertexCount, (unsigned)instanceCount);
}

void Graphics::DrawInstanced(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart, size_t instanceStart,
    size_t instanceCount)
{
    if (!indexBuffer)
        return;

    size_t indexSize = indexBuffer->IndexSize();

    PrepareDraw(true, instanceStart);
    if (!vertexStart)
    {
        glDrawElementsInstanced(glPrimitiveType[type], (unsigned)indexCount, indexSize == sizeof(unsigned short) ?
            GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(indexStart * indexSize), (unsigned)instanceCount);
    }
    else
    {
        glDrawElementsInstancedBaseVertex(glPrimitiveType[type], (unsigned)indexCount, indexSize == sizeof(unsigned short) ?
            GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(indexStart * indexSize), (unsigned)instanceCount, 
            (unsigned)vertexStart);
    }
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
            {
                if (shaderProgram == it->second)
                    shaderProgram = nullptr;
                it = shaderPrograms.Erase(it);
            }
            else
                ++it;
        }
    }
    else
    {
        for (auto it = shaderPrograms.Begin(); it != shaderPrograms.End();)
        {
            if (it->first.second == shader)
            {
                if (shaderProgram == it->second)
                    shaderProgram = nullptr;
                it = shaderPrograms.Erase(it);
            }
            else
                ++it;
        }
    }
}

void Graphics::BindVBO(unsigned vbo)
{
    if (vbo != boundVBO)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        boundVBO = vbo;
    }
}

void Graphics::HandleResize(WindowResizeEvent& event)
{
    // Handle windowed mode resize
    if (!fullscreen)
    {
        // Reset viewport in case the application does not set it
        if (context)
        {
            backbufferSize = event.size;
            ResetRenderTargets();
            SetViewport(IntRect(0, 0, backbufferSize.x, backbufferSize.y));
        }
    }
}

void Graphics::PrepareDraw(bool instanced, size_t instanceStart)
{
    if (vertexAttributesDirty && shaderProgram)
    {
        usedVertexAttributes = 0;

        for (auto it = attributeBySemantic.Begin(); it != attributeBySemantic.End(); ++it)
            it->Clear();

        const Vector<VertexAttribute>& attributes = shaderProgram->Attributes();
        for (auto it = attributes.Begin(); it != attributes.End(); ++it)
        {
            const VertexAttribute& attribute = *it;
            Vector<unsigned>& attributeVector = attributeBySemantic[attribute.semantic];
            unsigned char index = attribute.index;

            // Mark semantic as required
            size_t size = attributeVector.Size();
            if (size <= index)
            {
                attributeVector.Resize(index + 1);
                // If there are gaps (eg. texcoord1 used without texcoord0), fill them with illegal index
                for (size_t j = size; j < index; ++j)
                    attributeVector[j] = M_MAX_UNSIGNED;
            }
            attributeVector[index] = attribute.location;
            usedVertexAttributes |= (1 << attribute.location);
        }

        vertexAttributesDirty = false;
        vertexBuffersDirty = true;
    }

    if (vertexBuffersDirty || instanced)
    {
        // Now go through currently bound vertex buffers and set the attribute pointers that are available & required
        for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        {
            if (vertexBuffers[i])
            {
                VertexBuffer* buffer = vertexBuffers[i];
                const Vector<VertexElement>& elements = buffer->Elements();

                for (auto it = elements.Begin(); it != elements.End(); ++it)
                {
                    const VertexElement& element = *it;
                    const Vector<unsigned>& attributeVector = attributeBySemantic[element.semantic];

                    // If making several instanced draw calls with the same vertex buffers, only need to update the instancing
                    // data attribute pointers
                    if (element.index < attributeVector.Size() && attributeVector[element.index] < M_MAX_UNSIGNED &&
                        (vertexBuffersDirty || (instanced && element.perInstance)))
                    {
                        unsigned location = attributeVector[element.index];
                        unsigned locationMask = 1 << location;

                        // Enable attribute if not enabled yet
                        if (!(enabledVertexAttributes & locationMask))
                        {
                            glEnableVertexAttribArray(location);
                            enabledVertexAttributes |= locationMask;
                        }

                        // Enable/disable instancing divisor as necessary
                        size_t dataStart = element.offset;
                        if (element.perInstance)
                        {
                            dataStart += instanceStart * buffer->VertexSize();
                            if (!(instancingVertexAttributes & locationMask))
                            {
                                glVertexAttribDivisorARB(location, 1);
                                instancingVertexAttributes |= locationMask;
                            }
                        }
                        else
                        {
                            if (instancingVertexAttributes & locationMask)
                            {
                                glVertexAttribDivisorARB(location, 0);
                                instancingVertexAttributes &= ~locationMask;
                            }
                        }

                        BindVBO(buffer->GLBuffer());
                        glVertexAttribPointer(location, elementGLComponents[element.type], elementGLType[element.type],
                            element.semantic == SEM_COLOR ? GL_TRUE : GL_FALSE, (unsigned)buffer->VertexSize(),
                            (const void *)dataStart);
                    }
                }
            }
        }

        vertexBuffersDirty = false;
    }

    // Finally disable unnecessary vertex attributes
    unsigned disableVertexAttributes = enabledVertexAttributes & (~usedVertexAttributes);
    unsigned location = 0;
    while (disableVertexAttributes)
    {
        if (disableVertexAttributes & 1)
        {
            glDisableVertexAttribArray(location);
            enabledVertexAttributes &= ~(1 << location);
        }
        ++location;
        disableVertexAttributes >>= 1;
    }
}

void Graphics::ResetState()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        vertexBuffers[i] = nullptr;

    enabledVertexAttributes = 0;
    usedVertexAttributes = 0;
    instancingVertexAttributes = 0;

    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        for (size_t j = 0; j < MAX_CONSTANT_BUFFERS; ++j)
            constantBuffers[i][j] = nullptr;
    }

    for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        textures[i] = nullptr;
        textureTargets[i] = 0;
    }

    indexBuffer = nullptr;
    vertexShader = nullptr;
    pixelShader = nullptr;
    shaderProgram = nullptr;
    blendState = nullptr;
    depthState = nullptr;
    rasterizerState = nullptr;
    vertexAttributesDirty = false;
    vertexBuffersDirty = false;
    primitiveType = MAX_PRIMITIVE_TYPES;
    scissorRect = IntRect();
    stencilRef = 0;
    activeTexture = 0;
    boundVBO = 0;
}

void RegisterGraphicsLibrary()
{
    Shader::RegisterObject();
    Texture::RegisterObject();
}

}
