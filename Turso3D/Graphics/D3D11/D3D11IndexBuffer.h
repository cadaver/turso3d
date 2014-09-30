// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../GPUObject.h"
#include "../GraphicsDefs.h"
#include "../../Base/AutoPtr.h"

namespace Turso3D
{

/// GPU buffer for index data.
class TURSO3D_API IndexBuffer : public GPUObject
{
public:
    /// Construct.
    IndexBuffer();
    /// Destruct.
    virtual ~IndexBuffer();

    /// Release the index buffer and CPU shadow data.
    virtual void Release();

    /// Define buffer with initial data. Non-dynamic buffer must specify data here, as it will be immutable after creation. Return true on success.
    bool Define(size_t numIndices, size_t indexSize, bool dynamic, bool shadow, const void* data);
    /// Redefine buffer data either completely or partially. Buffer must be dynamic. Return true on success.
    bool SetData(size_t firstIndex, size_t numIndices, const void* data);

    /// Return the D3D11 buffer.
    void* Buffer() const { return buffer; }
    /// Return CPU-side shadow data if exists.
    unsigned char* ShadowData() const { return shadowData.Get(); }
    /// Return number of indices.
    size_t NumIndices() const { return numIndices; }
    /// Return size of index in bytes.
    size_t IndexSize() const { return indexSize; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return dynamic; }

private:
    /// D3D11 buffer.
    void* buffer;
    /// CPU-side shadow data.
    AutoArrayPtr<unsigned char> shadowData;
    /// Number of indices.
    size_t numIndices;
    /// Size of index in bytes.
    size_t indexSize;
    /// Dynamic flag.
    bool dynamic;
};

}