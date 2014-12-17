// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "VertexBuffer.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

bool VertexBuffer::Define(ResourceUsage usage_, size_t numVertices_, const Vector<VertexElement>& elements_, bool useShadowData, const void* data)
{
    if (!numVertices_ || !elements_.Size())
    {
        LOGERROR("Can not define vertex buffer with no vertices or no elements");
        return false;
    }

    return Define(usage_, numVertices_, elements_.Size(), &elements_[0], useShadowData, data);
}

bool VertexBuffer::Define(ResourceUsage usage_, size_t numVertices_, size_t numElements_, const VertexElement* elements_, bool useShadowData, const void* data)
{
    PROFILE(DefineVertexBuffer);

    if (!numVertices_ || !numElements_ || !elements_)
    {
        LOGERROR("Can not define vertex buffer with no vertices or no elements");
        return false;
    }
    if (usage_ == USAGE_RENDERTARGET)
    {
        LOGERROR("Rendertarget usage is illegal for vertex buffers");
        return false;
    }
    if (usage_ == USAGE_IMMUTABLE && !data)
    {
        LOGERROR("Immutable vertex buffer must define initial data");
        return false;
    }

    for (size_t i = 0; i < numElements_; ++i)
    {
        if (elements_[i].type >= ELEM_MATRIX3X4)
        {
            LOGERROR("Matrix elements are not supported in vertex buffers");
            return false;
        }
    }

    Release();

    numVertices = numVertices_;
    usage = usage_;

    // Determine offset of elements and the vertex size & element hash
    vertexSize = 0;
    elementHash = 0;
    elements.Resize(numElements_);
    for (size_t i = 0; i < numElements_; ++i)
    {
        elements[i] = elements_[i];
        elements[i].offset = vertexSize;
        vertexSize += elementSizes[elements[i].type];
        elementHash |= ElementHash(i, elements[i].semantic);
    }

    // If buffer is reinitialized with the same shadow data, no need to reallocate
    if (useShadowData && (!data || data != shadowData.Get()))
    {
        shadowData = new unsigned char[numVertices * vertexSize];
        if (data)
            memcpy(shadowData.Get(), data, numVertices * vertexSize);
    }

    return Create(data);
}

}
