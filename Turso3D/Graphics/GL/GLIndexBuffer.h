// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Base/AutoPtr.h"
#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// GPU buffer for index data.
class TURSO3D_API IndexBuffer : public WeakRefCounted, public GPUObject
{
public:
    /// Construct.
    IndexBuffer();
    /// Destruct.
    ~IndexBuffer();

    /// Release the index buffer and CPU shadow data.
    void Release() override;
    /// Recreate the GPU resource after data loss.
    void Recreate() override;

    /// Define buffer. Immutable buffers must specify initial data here.  Return true on success.
    bool Define(ResourceUsage usage, size_t numIndices, size_t indexSize, bool useShadowData, const void* data = nullptr);
    /// Redefine buffer data either completely or partially. Not supported for immutable buffers. Return true on success.
    bool SetData(size_t firstIndex, size_t numIndices, const void* data);

    /// Return CPU-side shadow data if exists.
    unsigned char* ShadowData() const { return shadowData.Get(); }
    /// Return number of indices.
    size_t NumIndices() const { return numIndices; }
    /// Return size of index in bytes.
    size_t IndexSize() const { return indexSize; }
    /// Return resource usage type.
    ResourceUsage Usage() const { return usage; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return usage == USAGE_DYNAMIC; }
    /// Return whether is immutable.
    bool IsImmutable() const { return usage == USAGE_IMMUTABLE; }

    /// Return the OpenGL buffer identifier. Used internally and should not be called by portable application code.
    unsigned GLBuffer() const { return buffer; }

private:
    /// OpenGL buffer object identifier.
    unsigned buffer;
    /// CPU-side shadow data.
    AutoArrayPtr<unsigned char> shadowData;
    /// Number of indices.
    size_t numIndices;
    /// Size of index in bytes.
    size_t indexSize;
    /// Resource usage type.
    ResourceUsage usage;
};

}