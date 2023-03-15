// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/AutoPtr.h"
#include "../Object/Ptr.h"
#include "GraphicsDefs.h"

#include <vector>

/// GPU buffer for vertex data.
class VertexBuffer : public RefCounted
{
public:
    /// Construct. Graphics subsystem must have been initialized.
    VertexBuffer();
    /// Destruct.
    ~VertexBuffer();

    /// Define buffer. Return true on success.
    bool Define(ResourceUsage usage, size_t numVertices, const std::vector<VertexElement>& elements, const void* data = nullptr);
    /// Redefine buffer data either completely or partially. Return true on success.
    bool SetData(size_t firstVertex, size_t numVertices, const void* data, bool discard = false);
    /// Bind to use with the specified vertex attributes. No-op if already bound. Used also when defining or setting data.
    void Bind(unsigned attributeMask);

    /// Return number of vertices.
    size_t NumVertices() const { return numVertices; }
    /// Return number of vertex elements.
    size_t NumElements() const { return elements.size(); }
    /// Return vertex elements.
    const std::vector<VertexElement>& Elements() const { return elements; }
    /// Return size of vertex in bytes.
    size_t VertexSize() const { return vertexSize; }
    /// Return vertex attribute mask.
    unsigned Attributes() const { return attributes; }
    /// Return resource usage type.
    ResourceUsage Usage() const { return usage; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return usage == USAGE_DYNAMIC; }

    /// Return the OpenGL object identifier.
    unsigned GLBuffer() const { return buffer; }

    /// Calculate a vertex attribute mask from elements.
    static unsigned CalculateAttributeMask(const std::vector<VertexElement>& elements);
    /// Return size of vertex element.
    static size_t VertexElementSize(const VertexElement& element);

private:
    /// Create the GPU-side vertex buffer. Return true on success.
    bool Create(const void* data);
    /// Release the vertex buffer and CPU shadow data.
    void Release();

    /// OpenGL object identifier.
    unsigned buffer;
    /// Number of vertices.
    size_t numVertices;
    /// Size of vertex in bytes.
    size_t vertexSize;
    /// Vertex attribute bitmask.
    unsigned attributes;
    /// Resource usage type.
    ResourceUsage usage;
    /// Vertex elements.
    std::vector<VertexElement> elements;
};
