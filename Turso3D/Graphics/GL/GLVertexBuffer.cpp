// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLGraphics.h"
#include "GLVertexBuffer.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

VertexBuffer::VertexBuffer() :
    buffer(0),
    numVertices(0),
    vertexSize(0),
    elementHash(0),
    usage(USAGE_DEFAULT)
{
}

VertexBuffer::~VertexBuffer()
{
    Release();
}

void VertexBuffer::Release()
{
    if (graphics)
    {
        for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        {
            if (graphics->GetVertexBuffer(i) == this)
                graphics->SetVertexBuffer(i, 0);
        }
    }

    if (buffer)
    {
        if (graphics && graphics->BoundVBO() == buffer)
            graphics->BindVBO(0);

        glDeleteBuffers(1, &buffer);
        buffer = 0;
    }
}

void VertexBuffer::Recreate()
{
    if (numVertices)
    {
        // Also make a copy of the current vertex elements, as they are passed by reference and manipulated by Define()
        Vector<VertexElement> srcElements = elements;
        Define(usage, numVertices, srcElements, !shadowData.IsNull(), shadowData.Get());
        SetDataLost(!shadowData.IsNull());
    }
}

bool VertexBuffer::SetData(size_t firstVertex, size_t numVertices_, const void* data)
{
    PROFILE(UpdateVertexBuffer);

    if (!data)
    {
        LOGERROR("Null source data for updating vertex buffer");
        return false;
    }
    if (firstVertex + numVertices_ > numVertices)
    {
        LOGERROR("Out of bounds range for updating vertex buffer");
        return false;
    }
    if (buffer && usage == USAGE_IMMUTABLE)
    {
        LOGERROR("Can not update immutable vertex buffer");
        return false;
    }

    if (shadowData)
        memcpy(shadowData.Get() + firstVertex * vertexSize, data, numVertices_ * vertexSize);

    if (buffer)
    {
        graphics->BindVBO(buffer);
        if (numVertices_ == numVertices)
            glBufferData(GL_ARRAY_BUFFER, numVertices_ * vertexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        else
            glBufferSubData(GL_ARRAY_BUFFER, firstVertex * vertexSize, numVertices_ * vertexSize, data);
    }

    return true;
}

bool VertexBuffer::Create(const void* data)
{
    if (graphics && graphics->IsInitialized())
    {
        glGenBuffers(1, &buffer);
        if (!buffer)
        {
            LOGERROR("Failed to create vertex buffer");
            return false;
        }

        graphics->BindVBO(buffer);
        glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        LOGDEBUGF("Created vertex buffer numVertices %u vertexSize %u", (unsigned)numVertices, (unsigned)vertexSize);
    }

    return true;
}

}
