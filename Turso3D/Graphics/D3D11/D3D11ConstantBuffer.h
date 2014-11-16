// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Base/AutoPtr.h"
#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// Description of a shader constant.
struct TURSO3D_API Constant
{
    /// Construct empty.
    Constant() :
        numElements(1)
    {
    }

    /// Construct with type, name and optional number of elements.
    Constant(ElementType type_, const String& name_, size_t numElements_ = 1) :
        type(type_),
        name(name_),
        numElements(numElements_)
    {
    }

    /// Construct with type, name and optional number of elements.
    Constant(ElementType type_, const char* name_, size_t numElements_ = 1) :
        type(type_),
        name(name_),
        numElements(numElements_)
    {
    }

    /// Data type of constant.
    ElementType type;
    /// Name of constant.
    String name;
    /// Number of elements. Default 1.
    size_t numElements;
    /// Element size. Filled by ConstantBuffer.
    size_t elementSize;
    /// Offset from the beginning of the buffer. Filled by ConstantBuffer.
    size_t offset;
};

/// GPU buffer for shader constant data.
class TURSO3D_API ConstantBuffer : public WeakRefCounted, public GPUObject
{
public:
    /// Construct.
    ConstantBuffer();
    /// Destruct.
    ~ConstantBuffer();

    /// Release the buffer.
    void Release() override;

    /// Define the constants being used and create the GPU-side buffer. Return true on success.
    bool Define(const Vector<Constant>& srcConstants);
    /// Define the constants being used and create the GPU-side buffer. Return true on success.
    bool Define(size_t numConstants, const Constant* srcConstants);
    /// Set a constant by index. Optionally specify how many elements to update, default all. Return true on success.
    bool SetConstant(size_t index, void* data, size_t numElements = 0);
    /// Set a constant by name. Optionally specify how many elements to update, default all. Return true on success.
    bool SetConstant(const String& name, void* data, size_t numElements = 0);
    /// Set a constant by name. Optionally specify how many elements to update, default all. Return true on success.
    bool SetConstant(const char* name, void* data, size_t numElements = 0);
    /// Apply to the GPU-side buffer if has changes. Return true on success.
    bool Apply();
    /// Set a constant by index, template version.
    template <class T> bool SetConstant(size_t index, const T& data, size_t numElements = 0) { return SetConstant(index, (void*)&data, numElements); }
    /// Set a constant by name, template version.
    template <class T> bool SetConstant(const String& name, const T& data, size_t numElements = 0) { return SetConstant(name, (void*)&data, numElements); }
    /// Set a constant by name, template version.
    template <class T> bool SetConstant(const char* name, const T& data, size_t numElements = 0) { return SetConstant(name, (void*)&data, numElements); }

    /// Return number of constants.
    size_t NumConstants() const { return constants.Size(); }
    /// Return the constant descriptions.
    const Vector<Constant>& Constants() const { return constants; }
    /// Return the index of a constant, or NPOS if not found.
    size_t FindConstantIndex(const String& name);
    /// Return the index of a constant, or NPOS if not found.
    size_t FindConstantIndex(const char* name);
    /// Return total byte size of the buffer.
    size_t ByteSize() const { return byteSize; }
    /// Return whether buffer has unapplied changes.
    bool IsDirty() const { return dirty; }

    /// Return the D3D11 buffer. Used internally and should not be called by portable application code.
    void* BufferObject() const { return buffer; }

    /// Element sizes by type.
    static const size_t elementSize[];
    
    /// Index for "constant not found."
    static const size_t NPOS = (size_t)-1;

private:
    /// D3D11 buffer.
    void* buffer;
    /// Constant definitions.
    Vector<Constant> constants;
    /// CPU-side data where updates are collected before applying.
    AutoArrayPtr<unsigned char> shadowData;
    /// Total byte size.
    size_t byteSize;
    /// Dirty flag.
    bool dirty;
};

}