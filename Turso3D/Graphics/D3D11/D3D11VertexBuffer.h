// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../GPUObject.h"
#include "../GraphicsDefs.h"
#include "../../Base/AutoPtr.h"

namespace Turso3D
{

/// GPU buffer for vertex data.
class TURSO3D_API VertexBuffer : public GPUObject
{
public:
    /// Construct.
    VertexBuffer();
    /// Destruct.
    virtual ~VertexBuffer();

    /// Release the vertex buffer and CPU shadow data.
    virtual void Release();

    /// Define buffer with initial data. Non-dynamic buffer must specify data here, as it will be immutable after creation. Return true on success.
    bool Define(size_t numVertices, unsigned elementMask, bool dynamic, bool shadow, const void* data);
    /// Redefine buffer data either completely or partially. Buffer must be dynamic. Return true on success.
    bool SetData(size_t firstVertex, size_t numVertices, const void* data);

    /// Return the D3D11 buffer.
    void* Buffer() const { return buffer; }
    /// Return CPU-side shadow data if exists.
    unsigned char* ShadowData() const { return shadowData.Get(); }
    /// Return number of vertices.
    size_t NumVertices() const { return numVertices; }
    /// Return size of vertex in bytes.
    size_t VertexSize() const { return vertexSize; }
    /// Return element mask.
    unsigned ElementMask() const { return elementMask; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return dynamic; }
    /// Return vertex element offset in bytes from beginning of vertex.
    size_t ElementOffset(VertexElement element) const;

    /// Compute vertex size for a particular element mask.
    static size_t ComputeVertexSize(unsigned elementMask);
    /// Compute vertex element offset for a particular element mask.
    static size_t ElementOffset(VertexElement element, unsigned elementMask);

    /// Vertex element sizes.
    static const size_t elementSize[];
    /// Vertex element D3D11 semantics.
    static const char* elementSemantic[];
    /// Vertex element D3D11 semantic indices (when more than one of the same element)
    static const unsigned VertexBuffer::elementSemanticIndex[];
    /// Vertex element D3D11 formats
    static const unsigned VertexBuffer::elementFormat[];

private:
    /// D3D11 buffer.
    void* buffer;
    /// CPU-side shadow data.
    AutoArrayPtr<unsigned char> shadowData;
    /// Number of vertices.
    size_t numVertices;
    /// Size of vertex in bytes.
    size_t vertexSize;
    /// Vertex element mask.
    unsigned elementMask;
    /// Dynamic flag.
    bool dynamic;
};

}