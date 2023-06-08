// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "Graphics.h"
#include "RenderBuffer.h"
#include "Texture.h"

#include <glew.h>
#include <tracy/Tracy.hpp>

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
    if (Object::Subsystem<Graphics>())
        Release();
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
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample, Texture::glInternalFormats[format], size.x, size.y);
    else
        glRenderbufferStorage(GL_RENDERBUFFER, Texture::glInternalFormats[format], size.x, size.y);
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

void RenderBuffer::Release()
{
    if (buffer)
    {
        glDeleteRenderbuffers(1, &buffer);
        buffer = 0;
    }
}
