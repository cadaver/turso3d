// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Base/AutoPtr.h"
#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// GPU buffer for shader constant data.
class TURSO3D_API ConstantBuffer : public RefCounted, public GPUObject
{
public:
    /// Construct.
    ConstantBuffer();
    /// Destruct.
    ~ConstantBuffer();

    /// Release the buffer.
    void Release() override;
    /// Recreate the GPU resource after data loss.
    void Recreate() override;

    /// Load from JSON data. Return true on success.
    bool LoadJSON(const JSONValue& source);
    /// Save as JSON data.
    void SaveJSON(JSONValue& dest);
    /// Define the constants being used and create the GPU-side buffer. Return true on success.
    bool Define(ResourceUsage usage, const Vector<Constant>& srcConstants);
    /// Define the constants being used and create the GPU-side buffer. Return true on success.
    bool Define(ResourceUsage usage, size_t numConstants, const Constant* srcConstants);
    /// Set a constant by index. Optionally specify how many elements to update, default all. Return true on success.
    bool SetConstant(size_t index, const void* data, size_t numElements = 0);
    /// Set a constant by name. Optionally specify how many elements to update, default all. Return true on success.
    bool SetConstant(const String& name, const void* data, size_t numElements = 0);
    /// Set a constant by name. Optionally specify how many elements to update, default all. Return true on success.
    bool SetConstant(const char* name, const void* data, size_t numElements = 0);
    /// Apply to the GPU-side buffer if has changes. Can only be used once on an immutable buffer. Return true on success.
    bool Apply();
    /// Set a constant by index, template version.
    template <class T> bool SetConstant(size_t index, const T& data, size_t numElements = 0) { return SetConstant(index, (const void*)&data, numElements); }
    /// Set a constant by name, template version.
    template <class T> bool SetConstant(const String& name, const T& data, size_t numElements = 0) { return SetConstant(name, (const void*)&data, numElements); }
    /// Set a constant by name, template version.
    template <class T> bool SetConstant(const char* name, const T& data, size_t numElements = 0) { return SetConstant(name, (const void*)&data, numElements); }

    /// Return number of constants.
    size_t NumConstants() const { return constants.Size(); }
    /// Return the constant descriptions.
    const Vector<Constant>& Constants() const { return constants; }
    /// Return the index of a constant, or NPOS if not found.
    size_t FindConstantIndex(const String& name) const;
    /// Return the index of a constant, or NPOS if not found.
    size_t FindConstantIndex(const char* name) const;
    /// Return pointer to the constant value data, or null if not found.
    const unsigned char* ConstantData(size_t index) const;
    /// Return pointer to the constant value data, or null if not found.
    const unsigned char* ConstantData(const String& name) const;
    /// Return pointer to the constant value data, or null if not found.
    const unsigned char* ConstantData(const char* name) const;
    /// Return total byte size of the buffer.
    size_t ByteSize() const { return byteSize; }
    /// Return whether buffer has unapplied changes.
    bool IsDirty() const { return dirty; }
    /// Return resource usage type.
    ResourceUsage Usage() const { return usage; }
    /// Return whether is dynamic.
    bool IsDynamic() const { return usage == USAGE_DYNAMIC; }
    /// Return whether is immutable.
    bool IsImmutable() const { return usage == USAGE_IMMUTABLE; }

    /// Return the OpenGL buffer identifier. Used internally and should not be called by portable application code.
    unsigned GLBuffer() const { return buffer; }

    /// Index for "constant not found."
    static const size_t NPOS = (size_t)-1;

private:
    /// Actually create the constant buffer. Called on the first Apply() if the buffer is immutable. Return true on success.
    bool Create(const void* data = nullptr);

    /// OpenGL buffer object identifier.
    unsigned buffer;
    /// Constant definitions.
    Vector<Constant> constants;
    /// CPU-side data where updates are collected before applying.
    AutoArrayPtr<unsigned char> shadowData;
    /// Total byte size.
    size_t byteSize;
    /// Resource usage type.
    ResourceUsage usage;
    /// Dirty flag.
    bool dirty;
};

}