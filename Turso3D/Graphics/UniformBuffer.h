// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/AutoPtr.h"
#include "../Object/Ptr.h"
#include "GraphicsDefs.h"

/// GPU buffer for shader program uniform data. Currently used for per-view camera parameters, Forward+ light data, skinning matrices and materials. Not recommended to be used for small rapidly changing data like object's world matrix; bare uniforms will perform better.
class UniformBuffer : public RefCounted
{
public:
    /// Construct. Graphics subsystem must have been initialized.
    UniformBuffer();
    /// Destruct.
    ~UniformBuffer();

    /// Define GPU uniform buffer with byte size. Return true on success.
    bool Define(ResourceUsage usage, size_t size, const void* data = nullptr);
    /// Redefine buffer data either completely or partially. Return true on success.
    bool SetData(size_t offset, size_t numBytes, const void* data, bool discard = false);
    /// Bind to use at a specific shader slot. No-op if already bound.
    void Bind(size_t index);
    /// Destroy the GPU uniform buffer.
    void Destroy();

    /// Return size of buffer in bytes.
    size_t Size() const { return size; }
    /// Return resource usage type.
    ResourceUsage Usage() const { return usage; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return usage == USAGE_DYNAMIC; }

    /// Return the OpenGL object identifier.
    unsigned GLBuffer() const { return buffer; }

    /// Unbind a slot.
    static void Unbind(size_t index);

private:
    /// Create the GPU-side uniform buffer. Return true on success.
    bool Create(const void* data);

    /// OpenGL object identifier.
    unsigned buffer;
    /// Buffer size in bytes.
    size_t size;
    /// Resource usage type.
    ResourceUsage usage;
};
