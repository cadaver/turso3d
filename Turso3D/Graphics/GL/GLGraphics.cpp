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

static const unsigned elementGLTypes[] =
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

static const unsigned glPrimitiveTypes[] =
{
    0,
    GL_POINTS,
    GL_LINES,
    GL_LINE_STRIP,
    GL_TRIANGLES,
    GL_TRIANGLE_STRIP
};

static const unsigned glBlendFactors[] =
{
    0,
    GL_ZERO,
    GL_ONE,
    GL_SRC_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA_SATURATE,
};

static const unsigned glBlendOps[] =
{
    0,
    GL_FUNC_ADD,
    GL_FUNC_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT,
    GL_MIN,
    GL_MAX
};

static const unsigned glCompareFuncs[] =
{
    0,
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS,
};

static const unsigned glStencilOps[] =
{
    0,
    GL_KEEP,
    GL_ZERO,
    GL_REPLACE,
    GL_INCR,
    GL_DECR,
    GL_INVERT,
    GL_INCR_WRAP,
    GL_DECR_WRAP,
};

static const unsigned glFillModes[] =
{
    0,
    0,
    GL_LINE,
    GL_FILL
};

static unsigned MAX_FRAMEBUFFER_AGE = 16;

/// OpenGL framebuffer.
class Framebuffer
{
public:
    /// Construct.
    Framebuffer() :
        depthStencil(nullptr),
        drawBuffers(0),
        framesSinceUse(0),
        firstUse(true)
    {
        glGenFramebuffers(1, &buffer);
        for (size_t i = 0; i < MAX_RENDERTARGETS; ++i)
            renderTargets[i] = nullptr;
    }

    /// Destruct.
    ~Framebuffer()
    {
        glDeleteFramebuffers(1, &buffer);
    }

    /// OpenGL FBO handle.
    unsigned buffer;
    /// Color rendertargets bound to this FBO.
    Texture* renderTargets[MAX_RENDERTARGETS];
    /// Depth-stencil texture bound to this FBO.
    Texture* depthStencil;
    /// Enabled draw buffers.
    unsigned drawBuffers;
    /// Time since use in frames.
    unsigned framesSinceUse;
    /// First use flag, for setting up readbuffers.
    bool firstUse;
};

Graphics::Graphics() :
    backbufferSize(IntVector2::ZERO),
    renderTargetSize(IntVector2::ZERO),
    attributesBySemantic(MAX_ELEMENT_SEMANTICS),
    vsync(false)
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
    if (!window->SetSize(width, height, fullscreen, resizable))
        return false;

    if (!context)
    {
        if (!CreateContext())
            return false;

        backbufferSize = window->Size();
        ResetRenderTargets();
        ResetViewport();
    }

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
    CleanupFramebuffers();
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
        renderTargets[i] = (renderTargets_[i] && renderTargets_[i]->IsRenderTarget()) ? renderTargets_[i] : nullptr;

    for (size_t i = renderTargets_.Size(); i < MAX_RENDERTARGETS; ++i)
        renderTargets[i] = nullptr;

    depthStencil = (depthStencil_ && depthStencil_->IsDepthStencil()) ? depthStencil_ : nullptr;

    if (renderTargets[0])
        renderTargetSize = IntVector2(renderTargets[0]->Width(), renderTargets[0]->Height());
    else if (depthStencil)
        renderTargetSize = IntVector2(depthStencil->Width(), depthStencil->Height());
    else
        renderTargetSize = backbufferSize;

    framebufferDirty = true;
}

void Graphics::SetViewport(const IntRect& viewport_)
{
    PrepareFramebuffer();

    /// \todo Implement a member function in IntRect for clipping
    viewport.left = Clamp(viewport_.left, 0, renderTargetSize.x - 1);
    viewport.top = Clamp(viewport_.top, 0, renderTargetSize.y - 1);
    viewport.right = Clamp(viewport_.right, viewport.left + 1, renderTargetSize.x);
    viewport.bottom = Clamp(viewport_.bottom, viewport.top + 1, renderTargetSize.y);

    // Use Direct3D convention with the vertical coordinates ie. 0 is top
    glViewport(viewport.left, renderTargetSize.y - viewport.bottom, viewport.Width(), viewport.Height());
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
            glBindTexture(target, texture->GLTexture());
            textureTargets[index] = target;
        }
        else if (textureTargets[index])
        {
            glBindTexture(textureTargets[index], 0);
            textureTargets[index] = 0;
        }
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
        blendState = state;
        blendStateDirty = true;
    }
}

void Graphics::SetDepthState(DepthState* state, unsigned char stencilRef_)
{
    if (state != depthState || stencilRef_ != stencilRef)
    {
        depthState = state;
        depthStateDirty = true;
        stencilRef = stencilRef_;
    }
}

void Graphics::SetRasterizerState(RasterizerState* state)
{
    if (state != rasterizerState)
    {
        rasterizerState = state;
        rasterizerStateDirty = true;
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

        // Use Direct3D convention with the vertical coordinates ie. 0 is top
        glScissor(scissorRect.left, renderTargetSize.y - scissorRect.bottom, scissorRect.Width(), scissorRect.Height());
    }
}

void Graphics::ResetRenderTargets()
{
    SetRenderTarget(nullptr, nullptr);
}

void Graphics::ResetViewport()
{
    SetViewport(IntRect(0, 0, renderTargetSize.x, renderTargetSize.y));
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

void Graphics::ResetTextures()
{
    for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
        SetTexture(i, nullptr);
}

void Graphics::Clear(unsigned clearFlags, const Color& clearColor, float clearDepth, unsigned char clearStencil)
{
    PrepareFramebuffer();

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

    if ((clearFlags & CLEAR_COLOR) && colorWriteMask != COLORMASK_ALL)
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if ((clearFlags & CLEAR_DEPTH) && !depthWrite)
        glDepthMask(GL_TRUE);
    if ((clearFlags & CLEAR_STENCIL) && stencilWriteMask != 0xff)
        glStencilMask(0xff);

    glClear(glFlags);

    if ((clearFlags & CLEAR_COLOR) && colorWriteMask != COLORMASK_ALL)
    {
        glColorMask(
            (colorWriteMask & COLORMASK_R) ? GL_TRUE : GL_FALSE,
            (colorWriteMask & COLORMASK_G) ? GL_TRUE : GL_FALSE,
            (colorWriteMask & COLORMASK_B) ? GL_TRUE : GL_FALSE,
            (colorWriteMask & COLORMASK_A) ? GL_TRUE : GL_FALSE
        );
    }
    if ((clearFlags & CLEAR_DEPTH) && !depthWrite)
        glDepthMask(GL_FALSE);
    if ((clearFlags & CLEAR_STENCIL) && stencilWriteMask != 0xff)
        glStencilMask(stencilWriteMask);
}

void Graphics::Draw(PrimitiveType type, size_t vertexStart, size_t vertexCount)
{
    PrepareDraw();
    glDrawArrays(glPrimitiveTypes[type], (unsigned)vertexStart, (unsigned)vertexCount);
}

void Graphics::Draw(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart)
{
    if (!indexBuffer)
        return;
    
    size_t indexSize = indexBuffer->IndexSize();

    PrepareDraw();
    if (!vertexStart)
    {
        glDrawElements(glPrimitiveTypes[type], (unsigned)indexCount, indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT :
            GL_UNSIGNED_INT, (const void*)(indexStart * indexSize));
    }
    else
    {
        glDrawElementsBaseVertex(glPrimitiveTypes[type], (unsigned)indexCount, indexSize == sizeof(unsigned short) ?
            GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(indexStart * indexSize), (unsigned)vertexStart);
    }

}

void Graphics::DrawInstanced(PrimitiveType type, size_t vertexStart, size_t vertexCount, size_t instanceStart, size_t
    instanceCount)
{
    PrepareDraw(true, instanceStart);
    glDrawArraysInstanced(glPrimitiveTypes[type], (unsigned)vertexStart, (unsigned)vertexCount, (unsigned)instanceCount);
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
        glDrawElementsInstanced(glPrimitiveTypes[type], (unsigned)indexCount, indexSize == sizeof(unsigned short) ?
            GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(indexStart * indexSize), (unsigned)instanceCount);
    }
    else
    {
        glDrawElementsInstancedBaseVertex(glPrimitiveTypes[type], (unsigned)indexCount, indexSize == sizeof(unsigned short) ?
            GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(indexStart * indexSize), (unsigned)instanceCount, 
            (unsigned)vertexStart);
    }
}

bool Graphics::IsInitialized() const
{
    return window->IsOpen() && context;
}

bool Graphics::IsFullscreen() const
{
    return window->IsFullscreen();
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

void Graphics::CleanupFramebuffers(Texture* texture)
{
    if (!texture)
        return;

    for (auto it = framebuffers.Begin(); it != framebuffers.End(); ++it)
    {
        Framebuffer* framebuffer = it->second;

        for (size_t i = 0; i < MAX_RENDERTARGETS; ++i)
        {
            if (framebuffer->renderTargets[i] == texture)
                framebuffer->renderTargets[i] = nullptr;
        }
        if (framebuffer->depthStencil == texture)
            framebuffer->depthStencil = nullptr;
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

bool Graphics::CreateContext()
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

    // These states are always enabled to match Direct3D
    glEnable(GL_POLYGON_OFFSET_LINE);
    glEnable(GL_POLYGON_OFFSET_FILL);

    // Set up texture data read/write alignment. It is important that this is done before uploading any texture data
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    return true;
}

void Graphics::HandleResize(WindowResizeEvent& event)
{
    // Reset viewport in case the application does not set it
    if (context)
    {
        backbufferSize = event.size;
        ResetRenderTargets();
        ResetViewport();
    }
}

void Graphics::CleanupFramebuffers()
{
    // Never clean up the framebuffer currently in use
    if (framebuffer)
        framebuffer->framesSinceUse = 0;

    for (auto it = framebuffers.Begin(); it != framebuffers.End();)
    {
        if (it->second->framesSinceUse > MAX_FRAMEBUFFER_AGE)
            it = framebuffers.Erase(it);
        else
            it->second->framesSinceUse++;
    }
}

void Graphics::PrepareFramebuffer()
{
    if (framebufferDirty)
    {
        framebufferDirty = false;
        
        unsigned newDrawBuffers = 0;
        bool useBackbuffer = true;

        for (size_t i = 0; i < MAX_RENDERTARGETS; ++i)
        {
            if (renderTargets[i])
            {
                useBackbuffer = false;
                newDrawBuffers |= (1 << i);
            }
        }
        if (depthStencil)
            useBackbuffer = false;

        if (useBackbuffer)
        {
            if (framebuffer)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                framebuffer = nullptr;
            }
            return;
        }

        // Search for a new framebuffer based on format & size, or create new
        ImageFormat format = FMT_NONE;
        if (renderTargets[0])
            format = renderTargets[0]->Format();
        else if (depthStencil)
            format = depthStencil->Format();
        unsigned long long key = (renderTargetSize.x << 16 | renderTargetSize.y) | (((unsigned long long)format) << 32);
        
        auto it = framebuffers.Find(key);
        if (it == framebuffers.End())
            it = framebuffers.Insert(MakePair(key, AutoPtr<Framebuffer>(new Framebuffer())));

        if (it->second != framebuffer)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, it->second->buffer);
            framebuffer = it->second;
        }

        framebuffer->framesSinceUse = 0;

        // Setup readbuffers & drawbuffers
        if (framebuffer->firstUse)
        {
            glReadBuffer(GL_NONE);
            framebuffer->firstUse = false;
        }

        if (newDrawBuffers != framebuffer->drawBuffers)
        {
            if (!newDrawBuffers)
                glDrawBuffer(GL_NONE);
            else
            {
                int drawBufferIds[MAX_RENDERTARGETS];
                unsigned drawBufferCount = 0;

                for (unsigned i = 0; i < MAX_RENDERTARGETS; ++i)
                {
                    if (newDrawBuffers & (1 << i))
                        drawBufferIds[drawBufferCount++] = GL_COLOR_ATTACHMENT0 + i;
                }
                glDrawBuffers(drawBufferCount, (const GLenum*)drawBufferIds);
            }

            framebuffer->drawBuffers = newDrawBuffers;
        }

        // Setup color attachments
        for (size_t i = 0; i < MAX_RENDERTARGETS; ++i)
        {
            if (renderTargets[i] != framebuffer->renderTargets[i])
            {
                if (renderTargets[i])
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, renderTargets[i]->GLTarget(),
                        renderTargets[i]->GLTexture(), 0);
                }
                else
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
                
                framebuffer->renderTargets[i] = renderTargets[i];
            }
        }

        // Setup depth & stencil attachments
        if (depthStencil != framebuffer->depthStencil)
        {
            if (depthStencil)
            {
                bool hasStencil = depthStencil->Format() == FMT_D24S8;
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthStencil->GLTarget(), 
                    depthStencil->GLTexture(), 0);
                if (hasStencil)
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, depthStencil->GLTarget(),
                        depthStencil->GLTexture(), 0);
                }
                else
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            }
            else
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
            }
        }
    }
}

void Graphics::PrepareDraw(bool instanced, size_t instanceStart)
{
    if (framebufferDirty)
        PrepareFramebuffer();

    if (vertexAttributesDirty && shaderProgram)
    {
        usedVertexAttributes = 0;

        for (auto it = attributesBySemantic.Begin(); it != attributesBySemantic.End(); ++it)
            it->Clear();

        const Vector<VertexAttribute>& attributes = shaderProgram->Attributes();
        for (auto it = attributes.Begin(); it != attributes.End(); ++it)
        {
            const VertexAttribute& attribute = *it;
            Vector<unsigned>& attributeVector = attributesBySemantic[attribute.semantic];
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
                    const Vector<unsigned>& attributeVector = attributesBySemantic[element.semantic];

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
                        glVertexAttribPointer(location, elementGLComponents[element.type], elementGLTypes[element.type],
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

    // Apply blend state
    if (blendStateDirty && blendState)
    {
        if (blendState->blendEnable != blendEnable)
        {
            if (blendState->blendEnable)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
            blendEnable = blendState->blendEnable;
        }

        if (blendState->srcBlend != srcBlend || blendState->destBlend != destBlend || blendState->srcBlendAlpha != srcBlendAlpha
            || blendState->destBlendAlpha != destBlendAlpha)
        {
            glBlendFuncSeparate(glBlendFactors[blendState->srcBlend], glBlendFactors[blendState->destBlend],
                glBlendFactors[blendState->srcBlendAlpha], glBlendFactors[blendState->destBlendAlpha]);
            srcBlend = blendState->srcBlend;
            destBlend = blendState->destBlend;
            srcBlendAlpha = blendState->srcBlendAlpha;
            destBlendAlpha = blendState->destBlendAlpha;
        }

        if (blendState->blendOp != blendOp || blendState->blendOpAlpha != blendOpAlpha)
        {
            glBlendEquationSeparate(glBlendOps[blendState->blendOp], glBlendOps[blendState->blendOpAlpha]);
            blendOp = blendState->blendOp;
            blendOpAlpha = blendState->blendOpAlpha;
        }

        if (blendState->colorWriteMask != colorWriteMask)
        {
            glColorMask(
                (blendState->colorWriteMask & COLORMASK_R) ? GL_TRUE : GL_FALSE,
                (blendState->colorWriteMask & COLORMASK_G) ? GL_TRUE : GL_FALSE,
                (blendState->colorWriteMask & COLORMASK_B) ? GL_TRUE : GL_FALSE,
                (blendState->colorWriteMask & COLORMASK_A) ? GL_TRUE : GL_FALSE
            );
            colorWriteMask = blendState->colorWriteMask;
        }

        if (blendState->alphaToCoverage != alphaToCoverage)
        {
            if (blendState->alphaToCoverage)
                glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
            else
                glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
            alphaToCoverage = blendState->alphaToCoverage;
        }

        blendStateDirty = false;
    }

    // Apply depth state
    if (depthStateDirty && depthState)
    {
        if (depthState->depthEnable != depthEnable)
        {
            if (depthState->depthEnable)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);
            depthEnable = depthState->depthEnable;
        }

        if (depthState->depthWrite != depthWrite)
        {
            glDepthMask(depthState->depthWrite ? GL_TRUE : GL_FALSE);
            depthWrite = depthState->depthWrite;
        }

        if (depthState->depthFunc != depthFunc)
        {
            glDepthFunc(glCompareFuncs[depthState->depthFunc]);
            depthFunc = depthState->depthFunc;
        }

        if (depthState->stencilEnable != stencilEnable)
        {
            if (depthState->stencilEnable)
                glEnable(GL_STENCIL_TEST);
            else
                glDisable(GL_STENCIL_TEST);
            stencilEnable = depthState->stencilEnable;
        }

        if (stencilEnable)
        {
            // Avoid using the "separate" stencil state calls if front/back functions or operations are the same
            if (depthState->frontFunc == depthState->backFunc)
            {
                if (depthState->frontFunc != frontFunc || stencilRef != currentStencilRef || depthState->stencilReadMask !=
                    stencilReadMask)
                {
                    glStencilFunc(glCompareFuncs[depthState->frontFunc], stencilRef, depthState->stencilReadMask);
                    frontFunc = backFunc = depthState->frontFunc;
                    currentStencilRef = stencilRef;
                    stencilReadMask = depthState->stencilReadMask;
                }
            }
            else
            {
                // Note: as polygons use Direct3D convention (clockwise = front) reversed front/back faces are used here
                if (depthState->frontFunc != frontFunc || stencilRef != currentStencilRef || depthState->stencilReadMask !=
                    stencilReadMask)
                {
                    glStencilFuncSeparate(GL_BACK, glCompareFuncs[depthState->frontFunc], stencilRef, depthState->stencilReadMask);
                    frontFunc = depthState->frontFunc;
                }
                if (depthState->backFunc != backFunc || stencilRef != currentStencilRef || depthState->stencilReadMask !=
                    stencilReadMask)
                {
                    glStencilFuncSeparate(GL_FRONT, glCompareFuncs[depthState->backFunc], stencilRef, depthState->stencilReadMask);
                    backFunc = depthState->backFunc;
                }
                currentStencilRef = stencilRef;
                stencilReadMask = depthState->stencilReadMask;
            }

            if (depthState->stencilWriteMask != stencilWriteMask)
            {
                glStencilMask(depthState->stencilWriteMask);
                stencilWriteMask = depthState->stencilWriteMask;
            }

            if (depthState->frontFail == depthState->backFail && depthState->frontDepthFail == depthState->backDepthFail &&
                depthState->frontPass == depthState->backPass)
            {
                if (depthState->frontFail != frontFail || depthState->frontDepthFail != frontDepthFail || depthState->frontPass
                    != frontPass)
                {
                    glStencilOp(glStencilOps[depthState->frontFail], glStencilOps[depthState->frontDepthFail],
                        glStencilOps[depthState->frontPass]);
                    frontFail = backFail = depthState->frontFail;
                    frontDepthFail = backDepthFail = depthState->frontDepthFail;
                    frontPass = backPass = depthState->frontPass;
                }
            }
            else
            {
                if (depthState->frontFail != frontFail || depthState->frontDepthFail != frontDepthFail || depthState->frontPass !=
                    frontPass)
                {
                    glStencilOpSeparate(GL_BACK, glStencilOps[depthState->frontFail], glStencilOps[depthState->frontDepthFail],
                        glStencilOps[depthState->frontPass]);
                    frontFail = depthState->frontFail;
                    frontDepthFail = depthState->frontDepthFail;
                    frontPass = depthState->frontPass;
                }
                if (depthState->backFail != backFail || depthState->backDepthFail != backDepthFail || depthState->backPass != backPass)
                {
                    glStencilOpSeparate(GL_FRONT, glStencilOps[depthState->backFail], glStencilOps[depthState->backDepthFail],
                        glStencilOps[depthState->backPass]);
                    backFail = depthState->backFail;
                    backDepthFail = depthState->backDepthFail;
                    backPass = depthState->backPass;
                }
            }
        }

        depthStateDirty = false;
    }

    // Apply rasterizer state
    if (rasterizerStateDirty && rasterizerState)
    {
        if (rasterizerState->fillMode != fillMode)
        {
            glPolygonMode(GL_FRONT_AND_BACK, glFillModes[rasterizerState->fillMode]);
            fillMode = rasterizerState->fillMode;
        }

        if (rasterizerState->cullMode != cullMode)
        {
            if (rasterizerState->cullMode == CULL_NONE)
                glDisable(GL_CULL_FACE);
            else
            {
                if (cullMode == CULL_NONE)
                    glEnable(GL_CULL_FACE);
                // Note: as polygons use Direct3D convention (clockwise = front) reversed front/back faces are used here
                glCullFace(rasterizerState->cullMode == CULL_BACK ? GL_FRONT : GL_BACK);
            }
            cullMode = rasterizerState->cullMode;
        }

        if (rasterizerState->depthBias != depthBias || rasterizerState->slopeScaledDepthBias != slopeScaledDepthBias)
        {
            /// \todo Check if this matches Direct3D
            glPolygonOffset(rasterizerState->slopeScaledDepthBias + 1.0f, (float)rasterizerState->depthBias);
            depthBias = rasterizerState->depthBias;
            slopeScaledDepthBias = rasterizerState->slopeScaledDepthBias;
        }

        if (rasterizerState->depthClipEnable != depthClipEnable)
        {
            if (rasterizerState->depthClipEnable)
                glDisable(GL_DEPTH_CLAMP);
            else
                glEnable(GL_DEPTH_CLAMP);
            depthClipEnable = rasterizerState->depthClipEnable;
        }

        if (rasterizerState->scissorEnable != scissorEnable)
        {
            if (rasterizerState->scissorEnable)
                glEnable(GL_SCISSOR_TEST);
            else
                glDisable(GL_SCISSOR_TEST);
            scissorEnable = rasterizerState->scissorEnable;
        }

        rasterizerStateDirty = false;
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
    framebuffer = nullptr;
    vertexAttributesDirty = false;
    vertexBuffersDirty = false;
    blendStateDirty = false;
    depthStateDirty = false;
    rasterizerStateDirty = false;
    framebufferDirty = false;
    scissorRect = IntRect();
    activeTexture = 0;
    boundVBO = 0;

    srcBlend = MAX_BLEND_FACTORS;
    destBlend = MAX_BLEND_FACTORS;
    blendOp = MAX_BLEND_OPS;
    srcBlendAlpha = MAX_BLEND_FACTORS;
    destBlendAlpha = MAX_BLEND_FACTORS;
    blendOpAlpha = MAX_BLEND_OPS;
    colorWriteMask = COLORMASK_ALL;
    blendEnable = false;
    alphaToCoverage = false;

    depthEnable = false;
    depthWrite = false;
    depthFunc = CMP_ALWAYS;
    stencilEnable = false;
    stencilReadMask = 0xff;
    stencilWriteMask = 0xff;
    frontFail = STENCIL_OP_KEEP;
    frontDepthFail = STENCIL_OP_KEEP;
    frontPass = STENCIL_OP_KEEP;
    frontFunc = CMP_ALWAYS;
    backFail = STENCIL_OP_KEEP;
    backDepthFail = STENCIL_OP_KEEP;
    backPass = STENCIL_OP_KEEP;
    backFunc = CMP_ALWAYS;
    stencilRef = 0;
    currentStencilRef = 0;

    fillMode = FILL_SOLID;
    cullMode = CULL_NONE;
    depthBias = 0;
    slopeScaledDepthBias = 0;
    depthClipEnable = true;
    scissorEnable = false;
}

void RegisterGraphicsLibrary()
{
    Shader::RegisterObject();
    Texture::RegisterObject();
}

}
