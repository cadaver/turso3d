// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "FrameBuffer.h"
#include "Graphics.h"
#include "RenderBuffer.h"
#include "Texture.h"

#include <glew.h>
#include <tracy/Tracy.hpp>

static FrameBuffer* boundDrawBuffer = nullptr;
static FrameBuffer* boundReadBuffer = nullptr;

FrameBuffer::FrameBuffer()
{
    assert(Object::Subsystem<Graphics>()->IsInitialized());

    glGenFramebuffers(1, &buffer);
}

FrameBuffer::~FrameBuffer()
{
    // Context may be gone at destruction time. In this case just no-op the cleanup
    if (Object::Subsystem<Graphics>())
        Release();
}

void FrameBuffer::Define(RenderBuffer* colorBuffer, RenderBuffer* depthStencilBuffer)
{
    ZoneScoped;

    Bind();

    IntVector2 size = IntVector2::ZERO;

    if (colorBuffer)
    {
        size = colorBuffer->Size();
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorBuffer->GLBuffer());
    }
    else
    {
        glDrawBuffer(GL_NONE);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
    }

    if (depthStencilBuffer)
    {
        if (size != IntVector2::ZERO && size != depthStencilBuffer->Size())
            LOGWARNING("Framebuffer color and depth dimensions don't match");
        else
            size = depthStencilBuffer->Size();

        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer->GLBuffer());
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilBuffer->Format() == FMT_D24S8 ? depthStencilBuffer->GLBuffer() : 0);
    }
    else
    {
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    }

    LOGDEBUGF("Defined framebuffer width %d height %d", size.x, size.y);
}

void FrameBuffer::Define(Texture* colorTexture, Texture* depthStencilTexture)
{
    ZoneScoped;

    Bind();

    IntVector2 size = IntVector2::ZERO;

    if (colorTexture && colorTexture->TexType() == TEX_2D)
    {
        size = colorTexture->Size2D();
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture->GLTexture(), 0);
    }
    else
    {
        glDrawBuffer(GL_NONE);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    }

    if (depthStencilTexture)
    {
        if (size != IntVector2::ZERO && size != depthStencilTexture->Size2D())
            LOGWARNING("Framebuffer color and depth dimensions don't match");
        else
            size = depthStencilTexture->Size2D();

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthStencilTexture->GLTexture(), 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthStencilTexture->Format() == FMT_D24S8 ? depthStencilTexture->GLTexture() : 0, 0);
    }
    else
    {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    }

    LOGDEBUGF("Defined framebuffer width %d height %d", size.x, size.y);
}

void FrameBuffer::Define(Texture* colorTexture, size_t cubeMapFace, Texture* depthStencilTexture)
{
    ZoneScoped;

    Bind();

    IntVector2 size = IntVector2::ZERO;

    if (colorTexture && colorTexture->TexType() == TEX_CUBE)
    {
        size = colorTexture->Size2D();
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)cubeMapFace, colorTexture->GLTexture(), 0);
    }
    else
    {
        glDrawBuffer(GL_NONE);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    }

    if (depthStencilTexture)
    {
        if (size != IntVector2::ZERO && size != depthStencilTexture->Size2D())
            LOGWARNING("Framebuffer color and depth dimensions don't match");
        else
            size = depthStencilTexture->Size2D();

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthStencilTexture->GLTexture(), 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthStencilTexture->Format() == FMT_D24S8 ? depthStencilTexture->GLTexture() : 0, 0);
    }
    else
    {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    }

    LOGDEBUGF("Defined framebuffer width %d height %d from cube texture", size.x, size.y);
}

void FrameBuffer::Define(const std::vector<Texture*>& colorTextures, Texture* depthStencilTexture)
{
    ZoneScoped;

    Bind();

    IntVector2 size = IntVector2::ZERO;

    std::vector<GLenum> drawBufferIds;
    for (size_t i = 0; i < colorTextures.size(); ++i)
    {
        if (colorTextures[i] && colorTextures[i]->TexType() == TEX_2D)
        {
            if (size != IntVector2::ZERO && size != colorTextures[i]->Size2D())
                LOGWARNING("Framebuffer color dimensions don't match");
            else
                size = colorTextures[i]->Size2D();

            drawBufferIds.push_back(GL_COLOR_ATTACHMENT0 + (GLenum)i);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_TEXTURE_2D, colorTextures[i]->GLTexture(), 0);
        }
        else
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLenum)i, GL_TEXTURE_2D, 0, 0);
    }

    if (drawBufferIds.size())
        glDrawBuffers((GLsizei)drawBufferIds.size(), &drawBufferIds[0]);
    else
        glDrawBuffer(GL_NONE);

    if (depthStencilTexture)
    {
        if (size != IntVector2::ZERO && size != depthStencilTexture->Size2D())
            LOGWARNING("Framebuffer color and depth dimensions don't match");
        else
            size = depthStencilTexture->Size2D();

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthStencilTexture->GLTexture(), 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthStencilTexture->Format() == FMT_D24S8 ? depthStencilTexture->GLTexture() : 0, 0);
    }
    else
    {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    }

    LOGDEBUGF("Defined MRT framebuffer width %d height %d", size.x, size.y);
}

void FrameBuffer::Bind()
{
    if (!buffer || boundDrawBuffer == this)
        return;

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffer);
    boundDrawBuffer = this;
}

void FrameBuffer::Bind(FrameBuffer* draw, FrameBuffer* read)
{
    if (boundDrawBuffer != draw)
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw ? draw->buffer : 0);
        boundDrawBuffer = draw;
    }

    if (boundReadBuffer != read)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read ? read->buffer : 0);
        boundReadBuffer = read;
    }
}

void FrameBuffer::Unbind()
{
    if (boundDrawBuffer)
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        boundDrawBuffer = nullptr;
    }
    if (boundReadBuffer)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        boundReadBuffer = nullptr;
    }
}

void FrameBuffer::Release()
{
    if (buffer)
    {
        if (boundDrawBuffer == this || boundReadBuffer == this)
            FrameBuffer::Unbind();

        glDeleteFramebuffers(1, &buffer);
        buffer = 0;
    }
}
