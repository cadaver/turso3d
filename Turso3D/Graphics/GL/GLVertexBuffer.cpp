// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Math/Matrix3x4.h"
#include "GLGraphics.h"
#include "GLVertexBuffer.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

const size_t VertexBuffer::elementSize[] =
{
    sizeof(int),
    sizeof(float),
    sizeof(Vector2),
    sizeof(Vector3),
    sizeof(Vector4),
    sizeof(unsigned),
    sizeof(Matrix3x4),
    sizeof(Matrix4)
};

const char* VertexBuffer::elementSemantic[] = {
    "POSITION",
    "NORMAL",
    "BINORMAL",
    "TANGENT",
    "TEXCOORD",
    "COLOR",
    "BLENDWEIGHT",
    "BLENDINDICES",
    nullptr
};

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
        glDeleteBuffers(1, &buffer);
        buffer = 0;
    }

    shadowData.Reset();
    elements.Clear();
    numVertices = 0;
    vertexSize = 0;
    elementHash = 0;
}

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
            LOGERROR("Matrix elements are not allowed in vertex buffers");
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
        vertexSize += elementSize[elements[i].type];
        elementHash |= ElementHash(i, elements[i].semantic);
    }

    if (useShadowData)
    {
        shadowData = new unsigned char[numVertices * vertexSize];
        if (data)
            memcpy(shadowData.Get(), data, numVertices * vertexSize);
    }

    if (graphics && graphics->IsInitialized())
    {
        glGenBuffers(1, &buffer);
        if (!buffer)
        {
            LOGERROR("Failed to create vertex buffer");
            return false;
        }

        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        LOGDEBUGF("Created vertex buffer numVertices %u vertexSize %u", (unsigned)numVertices, (unsigned)vertexSize);
    }

    return true;
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
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        if (numVertices_ == numVertices)
            glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        else
            glBufferSubData(GL_ARRAY_BUFFER, firstVertex * vertexSize, numVertices * vertexSize, data);
    }

    return true;
}

}