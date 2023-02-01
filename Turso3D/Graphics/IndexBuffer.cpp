// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Time/Profiler.h"
#include "Graphics.h"
#include "IndexBuffer.h"

#include <glew.h>
#include <cstring>

static IndexBuffer* boundIndexBuffer = nullptr;

IndexBuffer::IndexBuffer() :
    buffer(0),
    numIndices(0),
    indexSize(0),
    usage(USAGE_DEFAULT)
{
    assert(Object::Subsystem<Graphics>()->IsInitialized());
}

IndexBuffer::~IndexBuffer()
{
    // Context may be gone at destruction time. In this case just no-op the cleanup
    if (!Object::Subsystem<Graphics>())
        return;

    Release();
}

bool IndexBuffer::Define(ResourceUsage usage_, size_t numIndices_, size_t indexSize_, const void* data)
{
    PROFILE(DefineIndexBuffer);

    Release();

    if (!numIndices_)
    {
        LOGERROR("Can not define index buffer with no indices");
        return false;
    }
    if (indexSize_ != sizeof(unsigned) && indexSize_ != sizeof(unsigned short))
    {
        LOGERROR("Index buffer index size must be 2 or 4");
        return false;
    }

    numIndices = numIndices_;
    indexSize = indexSize_;
    usage = usage_;

    return Create(data);
}

void IndexBuffer::Release()
{
    if (buffer)
    {
        glDeleteBuffers(1, &buffer);
        buffer = 0;

        if (boundIndexBuffer == this)
            boundIndexBuffer = nullptr;
    }
}

bool IndexBuffer::SetData(size_t firstIndex, size_t numIndices_, const void* data, bool discard)
{
    PROFILE(UpdateIndexBuffer);

    if (!data)
    {
        LOGERROR("Null source data for updating index buffer");
        return false;
    }
    if (firstIndex + numIndices_ > numIndices)
    {
        LOGERROR("Out of bounds range for updating index buffer");
        return false;
    }

    if (buffer)
    {
        Bind(true);
        if (numIndices_ == numIndices)
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        else if (discard)
        {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, nullptr, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, firstIndex * indexSize, numIndices_ * indexSize, data);
        }
        else
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, firstIndex * indexSize, numIndices_ * indexSize, data);
    }

    return true;
}

bool IndexBuffer::Create(const void* data)
{
    glGenBuffers(1, &buffer);
    if (!buffer)
    {
        LOGERROR("Failed to create index buffer");
        return false;
    }

    Bind(true);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    LOGDEBUGF("Created index buffer numIndices %u indexSize %u", (unsigned)numIndices, (unsigned)indexSize);

    return true;
}

void IndexBuffer::Bind(bool force)
{
    if (!buffer || (boundIndexBuffer == this && !force))
        return;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    boundIndexBuffer = this;
}
