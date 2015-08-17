// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLGraphics.h"
#include "GLIndexBuffer.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

IndexBuffer::IndexBuffer() :
    buffer(0),
    numIndices(0),
    indexSize(0),
    usage(USAGE_DEFAULT)
{
}

IndexBuffer::~IndexBuffer()
{
    Release();
}

void IndexBuffer::Release()
{
    if (graphics && graphics->GetIndexBuffer() == this)
        graphics->SetIndexBuffer(nullptr);

    if (buffer)
    {
        glDeleteBuffers(1, &buffer);
        buffer = 0;
    }
}

void IndexBuffer::Recreate()
{
    if (numIndices)
    {
        Define(usage, numIndices, indexSize, !shadowData.IsNull(), shadowData.Get());
        SetDataLost(!shadowData.IsNull());
    }
}

bool IndexBuffer::SetData(size_t firstIndex, size_t numIndices_, const void* data)
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
    if (buffer && usage == USAGE_IMMUTABLE)
    {
        LOGERROR("Can not update immutable index buffer");
        return false;
    }

    if (shadowData)
        memcpy(shadowData.Get() + firstIndex * indexSize, data, numIndices_ * indexSize);

    if (buffer)
    {
        graphics->SetIndexBuffer(this);
        if (numIndices_ == numIndices)
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices_ * indexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        else
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, firstIndex * indexSize, numIndices_ * indexSize, data);
    }

    return true;
}

bool IndexBuffer::Create(const void* data)
{
    if (graphics && graphics->IsInitialized())
    {
        glGenBuffers(1, &buffer);
        if (!buffer)
        {
            LOGERROR("Failed to create index buffer");
            return false;
        }

        graphics->SetIndexBuffer(this);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        LOGDEBUGF("Created index buffer numIndices %u indexSize %u", (unsigned)numIndices, (unsigned)indexSize);
    }

    return true;
}

}
