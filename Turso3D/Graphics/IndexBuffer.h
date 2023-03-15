// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/AutoPtr.h"
#include "../Object/Ptr.h"
#include "GraphicsDefs.h"

/// GPU buffer for index data.
class IndexBuffer : public RefCounted
{
public:
    /// Construct. Graphics subsystem must have been initialized.
    IndexBuffer();
    /// Destruct.
    ~IndexBuffer();

    /// Define buffer. Return true on success.
    bool Define(ResourceUsage usage, size_t numIndices, size_t indexSize, const void* data = nullptr);
    /// Redefine buffer data either completely or partially. Return true on success.
    bool SetData(size_t firstIndex, size_t numIndices, const void* data, bool discard = false);
    /// Bind to use. No-op if already bound. Used also when defining or setting data.
    void Bind();

    /// Return number of indices.
    size_t NumIndices() const { return numIndices; }
    /// Return size of index in bytes.
    size_t IndexSize() const { return indexSize; }
    /// Return resource usage type.
    ResourceUsage Usage() const { return usage; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return usage == USAGE_DYNAMIC; }

    /// Return the OpenGL object identifier.
    unsigned GLBuffer() const { return buffer; }

    /// Return the index size of the currently bound buffer, or 0 if no buffer bound.
    static size_t BoundIndexSize();

private:
    /// Create the GPU-side index buffer. Return true on success.
    bool Create(const void* data);
    /// Release the index buffer and CPU shadow data.
    void Release();

    /// OpenGL object identifier.
    unsigned buffer;
    /// Number of indices.
    size_t numIndices;
    /// Size of index in bytes.
    size_t indexSize;
    /// Resource usage type.
    ResourceUsage usage;
};
