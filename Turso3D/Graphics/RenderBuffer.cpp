// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "Graphics.h"
#include "RenderBuffer.h"

#include <glew.h>
#include <tracy/Tracy.hpp>

static const GLenum glInternalFormats[] =
{
    0,
    GL_R8,
    GL_RG8,
    GL_RGBA8,
    GL_ALPHA,
    GL_R16,
    GL_RG16,
    GL_RGBA16,
    GL_R16F,
    GL_RG16F,
    GL_RGBA16F,
    GL_R32F,
    GL_RG32F,
    GL_RGB32F,
    GL_RGBA32F,
    GL_R32UI,
    GL_RG32UI,
    GL_RGBA32UI,
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT32,
    GL_DEPTH24_STENCIL8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};


RenderBuffer::RenderBuffer() :
    buffer(0),
    size(IntVector2::ZERO),
    format(FMT_NONE),
    multisample(0)
{
    assert(Object::Subsystem<Graphics>()->IsInitialized());
}

RenderBuffer::~RenderBuffer()
{
    // Context may be gone at destruction time. In this case just no-op the cleanup
    if (!Object::Subsystem<Graphics>())
        return;

    Release();
}

void RenderBuffer::Release()
{
    if (buffer)
    {
        glDeleteRenderbuffers(1, &buffer);
        buffer = 0;
    }
}

bool RenderBuffer::Define(const IntVector2& size_, ImageFormat format_, int multisample_)
{
    ZoneScoped;

    Release();

    if (format_ > FMT_DXT1)
    {
        LOGERROR("Compressed formats are unsupported for renderbuffers");
        return false;
    }
    if (size_.x < 1 || size_.y < 1)
    {
        LOGERROR("Renderbuffer must not have zero or negative size");
        return false;
    }

    if (multisample_ < 1)
        multisample_ = 1;

    glGenRenderbuffers(1, &buffer);
    if (!buffer)
    {
        size = IntVector2::ZERO;
        format = FMT_NONE;
        multisample = 0;

        LOGERROR("Failed to create renderbuffer");
        return false;
    }

    size = size_;
    format = format_;
    multisample = multisample_;

    // Clear previous error first to be able to check whether the data was successfully set
    glGetError();
    glBindRenderbuffer(GL_RENDERBUFFER, buffer);
    if (multisample > 1)
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample, glInternalFormats[format], size.x, size.y);
    else
        glRenderbufferStorage(GL_RENDERBUFFER, glInternalFormats[format], size.x, size.y);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // If we have an error now, the buffer was not created correctly
    if (glGetError() != GL_NO_ERROR)
    {
        Release();
        size = IntVector2::ZERO;
        format = FMT_NONE;

        LOGERROR("Failed to create renderbuffer");
        return false;
    }

    LOGDEBUGF("Created renderbuffer width %d height %d format %d", size.x, size.y, (int)format);

    return true;
}
