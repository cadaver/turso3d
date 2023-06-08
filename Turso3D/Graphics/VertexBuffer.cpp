// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "Graphics.h"
#include "VertexBuffer.h"

#include <glew.h>
#include <cstring>
#include <tracy/Tracy.hpp>

static unsigned boundAttributes = 0;
static VertexBuffer* boundVertexBuffer = nullptr;
static VertexBuffer* boundVertexAttribSource = nullptr;

static const unsigned baseAttributeIndex[] = 
{
    0,
    1,
    2,
    3,
    4,
    10,
    11
};

static const unsigned elementGLSizes[] =
{
    1,
    1,
    2,
    3,
    4,
    4
};

static const unsigned elementGLTypes[] =
{
    GL_INT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_BYTE,
};

VertexBuffer::VertexBuffer() :
    buffer(0),
    numVertices(0),
    vertexSize(0),
    attributes(0),
    usage(USAGE_DEFAULT)
{
    assert(Object::Subsystem<Graphics>()->IsInitialized());
}

VertexBuffer::~VertexBuffer()
{
    // Context may be gone at destruction time. In this case just no-op the cleanup
    if (Object::Subsystem<Graphics>())
        Release();
}

bool VertexBuffer::Define(ResourceUsage usage_, size_t numVertices_, const std::vector<VertexElement>& elements_, const void* data)
{
    ZoneScoped;

    Release();

    if (!numVertices_ || !elements_.size())
    {
        LOGERROR("Can not define vertex buffer with no vertices or no elements");
        return false;
    }

    numVertices = numVertices_;
    usage = usage_;

    // Determine offset of elements and the vertex size
    vertexSize = 0;
    elements.resize(elements_.size());
    for (size_t i = 0; i < elements.size(); ++i)
    {
        elements[i] = elements_[i];
        elements[i].offset = vertexSize;
        vertexSize += VertexElementSize(elements[i]);
    }

    attributes = CalculateAttributeMask(elements);

    return Create(data);
}

bool VertexBuffer::SetData(size_t firstVertex, size_t numVertices_, const void* data, bool discard)
{
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

    if (buffer)
    {
        Bind(0);

        if (numVertices_ == numVertices)
            glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        else if (discard)
        {
            glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, nullptr, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, firstVertex * vertexSize, numVertices_ * vertexSize, data);
        }
        else
            glBufferSubData(GL_ARRAY_BUFFER, firstVertex * vertexSize, numVertices_ * vertexSize, data);
    }

    return true;
}

void VertexBuffer::Bind(unsigned attributeMask)
{
    if (!buffer)
        return;

    // Special case attributeMask 0 used when binding for setting buffer content only or for instancing
    if (!attributeMask)
    {
        if (boundVertexBuffer != this)
        {
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            boundVertexBuffer = this;
        }
        return;
    }

    // Do not attempt to bind elements the current vertex buffer doesn't have
    attributeMask &= attributes;

    // If attributes already bound from this buffer, no-op
    if (attributeMask == boundAttributes && boundVertexAttribSource == this)
        return;

    if (boundVertexBuffer != this)
    {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        boundVertexBuffer = this;
    }

    unsigned usedAttributes = 0;

    for (size_t i = 0; i < elements.size(); ++i)
    {
        const VertexElement& element = elements[i];

        unsigned attributeIdx = baseAttributeIndex[element.semantic] + element.index;
        unsigned attributeBit = 1 << attributeIdx;
        if (!(attributeMask & attributeBit))
            continue;

        if (!(boundAttributes & attributeBit))
            glEnableVertexAttribArray(attributeIdx);

        glVertexAttribPointer(attributeIdx, elementGLSizes[element.type], elementGLTypes[element.type], element.semantic == SEM_COLOR ? GL_TRUE : GL_FALSE, 
            (GLsizei)vertexSize, reinterpret_cast<void*>(element.offset));

        usedAttributes |= attributeBit;
    }

    unsigned disableAttributes = boundAttributes & (~usedAttributes);
    unsigned disableIdx = 0;

    while (disableAttributes)
    {
        if (disableAttributes & 1)
            glDisableVertexAttribArray(disableIdx);
        disableAttributes >>= 1;
        ++disableIdx;
    }

    boundAttributes = usedAttributes;
    boundVertexBuffer = this;
    boundVertexAttribSource = this;
}

unsigned VertexBuffer::CalculateAttributeMask(const std::vector<VertexElement>& elements)
{
    unsigned attributes = 0;

    for (size_t i = 0; i < elements.size(); ++i)
    {
        unsigned attributeIdx = baseAttributeIndex[elements[i].semantic] + elements[i].index;
        unsigned attributeBit = 1 << attributeIdx;
        attributes |= attributeBit;
    }

    return attributes;
}

size_t VertexBuffer::VertexElementSize(const VertexElement& element)
{
    return elementSizes[element.type];
}

bool VertexBuffer::Create(const void* data)
{
    glGenBuffers(1, &buffer);
    if (!buffer)
    {
        LOGERROR("Failed to create vertex buffer");
        return false;
    }

    Bind(0);

    glBufferData(GL_ARRAY_BUFFER, numVertices * vertexSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    LOGDEBUGF("Created vertex buffer numVertices %u vertexSize %u", (unsigned)numVertices, (unsigned)vertexSize);

    if (boundVertexAttribSource == this)
        boundVertexAttribSource = nullptr;

    return true;
}

void VertexBuffer::Release()
{
    if (buffer)
    {
        glDeleteBuffers(1, &buffer);
        buffer = 0;

        if (boundVertexBuffer == this)
            boundVertexBuffer = nullptr;
        if (boundVertexAttribSource == this)
            boundVertexAttribSource = nullptr;
    }
}
