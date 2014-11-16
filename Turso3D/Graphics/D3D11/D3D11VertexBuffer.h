// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Base/AutoPtr.h"
#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// One vertex element in a vertex declaration.
struct TURSO3D_API VertexElement
{
    /// Default-construct.
    VertexElement() :
        type(ELEM_VECTOR3),
        semantic(SEM_POSITION),
        index(0),
        perInstance(false),
        offset(0)
    {
    }

    /// Construct with type, semantic, index and whether is per-instance data.
    VertexElement(ElementType type_, ElementSemantic semantic_, unsigned char index_ = 0, bool perInstance_ = false) :
        type(type_),
        semantic(semantic_),
        index(index_),
        perInstance(perInstance_),
        offset(0)
    {
    }

    /// Data type of element.
    ElementType type;
    /// Semantic of element.
    ElementSemantic semantic;
    /// Index of element, for example for multiple texcoords.
    unsigned char index;
    /// Per-instance flag.
    bool perInstance;
    /// Offset of element from vertex start. Filled by VertexBuffer.
    size_t offset;
};

/// GPU buffer for vertex data.
class TURSO3D_API VertexBuffer : public WeakRefCounted, public GPUObject
{
public:
    /// Construct.
    VertexBuffer();
    /// Destruct.
    ~VertexBuffer();

    /// Release the vertex buffer and CPU shadow data.
    void Release() override;

    /// Define buffer with initial data. Non-dynamic buffer must specify data here, as it will be immutable after creation. Return true on success.
    bool Define(size_t numVertices, const Vector<VertexElement>& elements, bool dynamic, bool shadow, const void* data);
    /// Define buffer with initial data. Non-dynamic buffer must specify data here, as it will be immutable after creation. Return true on success.
    bool Define(size_t numVertices, size_t numElements, const VertexElement* elements, bool dynamic, bool shadow, const void* data);
    /// Redefine buffer data either completely or partially. Buffer must be dynamic. Return true on success.
    bool SetData(size_t firstVertex, size_t numVertices, const void* data);

    /// Return CPU-side shadow data if exists.
    unsigned char* ShadowData() const { return shadowData.Get(); }
    /// Return number of vertices.
    size_t NumVertices() const { return numVertices; }
    /// Return number of vertex elements.
    size_t NumElements() const { return elements.Size(); }
    /// Return vertex elements.
    const Vector<VertexElement>& Elements() const { return elements; }
    /// Return size of vertex in bytes.
    size_t VertexSize() const { return vertexSize; }
    /// Return vertex declaration hash code.
    unsigned ElementHash() const { return elementHash; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return dynamic; }

    /// Return the D3D11 buffer. Used internally and should not be called by portable application code.
    void* BufferObject() const { return buffer; }

    /// Compute the hash code of one vertex element by index and semantic.
    static unsigned ElementHash(size_t index, ElementSemantic semantic) { return (semantic + 1) << (index * 3); }

    /// Vertex element size by element type.
    static const size_t elementSize[];
    /// Vertex element D3D11 format by element type.
    static const unsigned elementFormat[];
    /// Vertex element D3D11 semantic by element semantic.
    static const char* elementSemantic[];

private:
    /// D3D11 buffer.
    void* buffer;
    /// CPU-side shadow data.
    AutoArrayPtr<unsigned char> shadowData;
    /// Number of vertices.
    size_t numVertices;
    /// Size of vertex in bytes.
    size_t vertexSize;
    /// Vertex elements.
    Vector<VertexElement> elements;
    /// Vertex element hash code.
    unsigned elementHash;
    /// Dynamic flag.
    bool dynamic;
};

}