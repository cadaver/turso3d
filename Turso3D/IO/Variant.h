// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/HashMap.h"
#include "../Base/StringHash.h"
#include "../Base/Vector.h"
#include "../Base/WeakPtr.h"
#include "../Math/Color.h"
#include "../Math/IntRect.h"
#include "../Math/Matrix3.h"
#include "../Math/Matrix3x4.h"
#include "../Math/Rect.h"

namespace Turso3D
{

/// Variant's supported types.
enum VariantType
{
    VAR_NONE = 0,
    VAR_INT,
    VAR_BOOL,
    VAR_FLOAT,
    VAR_VECTOR2,
    VAR_VECTOR3,
    VAR_VECTOR4,
    VAR_QUATERNION,
    VAR_COLOR,
    VAR_STRING,
    VAR_BUFFER,
    VAR_VOIDPTR,
    VAR_RESOURCEREF,
    VAR_RESOURCEREFLIST,
    VAR_VARIANTVECTOR,
    VAR_VARIANTMAP,
    VAR_INTRECT,
    VAR_INTVECTOR2,
    VAR_PTR,
    VAR_MATRIX3,
    VAR_MATRIX3X4,
    VAR_MATRIX4,
    MAX_VAR_TYPES
};

/// Union for the possible variant values. Also stores non-POD objects such as String which must not exceed 16 bytes in size.
struct VariantValue
{
    union
    {
        char charValue;
        int intValue;
        bool boolValue;
        float floatValue;
        void* ptrValue;
        size_t padding[4];
    };
};

/// Typed resource reference.
struct TURSO3D_API ResourceRef
{
    /// Construct.
    ResourceRef()
    {
    }

    // Copy-construct.
    ResourceRef(const ResourceRef& ref) :
        type(ref.type),
        name(ref.name)
    {
    }

    /// Construct with type only and empty name.
    ResourceRef(StringHash type) :
        type(type)
    {
    }

    /// Construct with type and resource name.
    ResourceRef(StringHash type, const String& name_) :
        type(type),
        name(name_)
    {
    }

    /// Object type.
    StringHash type;
    /// Object name.
    String name;

    /// Test for equality with another reference.
    bool operator == (const ResourceRef& rhs) const { return type == rhs.type && name == rhs.name; }
    /// Test for inequality with another reference.
    bool operator != (const ResourceRef& rhs) const { return !(*this == rhs); }
};

/// %List of typed resource references.
struct TURSO3D_API ResourceRefList
{
    /// Construct.
    ResourceRefList()
    {
    }

    // Copy-construct.
    ResourceRefList(const ResourceRefList& refList) :
        type(refList.type),
        names(refList.names)
    {
    }

    /// Construct with type only.
    ResourceRefList(StringHash type) :
        type(type)
    {
    }

    /// Construct with type and name list.
    ResourceRefList(StringHash type, const Vector<String>& names_) :
        type(type),
        names(names_)
    {
    }

    /// Object type.
    StringHash type;
    /// List of object names.
    Vector<String> names;

    /// Test for equality with another reference list.
    bool operator == (const ResourceRefList& rhs) const { return type == rhs.type && names == rhs.names; }
    /// Test for inequality with another reference list.
    bool operator != (const ResourceRefList& rhs) const { return !(*this == rhs); }
};

class Variant;

/// Vector of variants.
typedef Vector<Variant> VariantVector;

/// Map of variants.
typedef HashMap<StringHash, Variant> VariantMap;

/// Variable that supports a fixed set of types.
class TURSO3D_API Variant
{
public:
    /// Construct empty.
    Variant() :
        type(VAR_NONE)
    {
    }

    /// Copy-construct.
    Variant(const Variant& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from integer.
    Variant(int value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from unsigned integer.
    Variant(unsigned value) :
        type(VAR_NONE)
    {
        *this = (int)value;
    }

    /// Construct from a string hash (convert to integer).
    Variant(const StringHash& value) :
        type(VAR_NONE)
    {
        *this = (int)value.Value();
    }

    /// Construct from a bool.
    Variant(bool value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a float.
    Variant(float value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a Vector2.
    Variant(const Vector2& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a Vector3.
    Variant(const Vector3& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a Vector4.
    Variant(const Vector4& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a quaternion.
    Variant(const Quaternion& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a color.
    Variant(const Color& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a string.
    Variant(const String& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a C string.
    Variant(const char* value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a buffer.
    Variant(const Vector<unsigned char>& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a pointer.
    Variant(void* value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a resource reference.
    Variant(const ResourceRef& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a resource reference list.
    Variant(const ResourceRefList& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a variant vector.
    Variant(const VariantVector& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a variant map.
    Variant(const VariantMap& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from an integer rect.
    Variant(const IntRect& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from an IntVector2.
    Variant(const IntVector2& value) :
        type(VAR_NONE)
    {
        *this = value;
    }
    
    /// Construct from a WeakRefCounted pointer. The object will be stored internally in a WeakPtr so that its expiration can be detected safely.
    Variant(WeakRefCounted* value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a Matrix3.
    Variant(const Matrix3& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a Matrix3x4.
    Variant(const Matrix3x4& value) :
        type(VAR_NONE)
    {
        *this = value;
    }

    /// Construct from a Matrix4.
    Variant(const Matrix4& value) :
        type(VAR_NONE)
    {
        *this = value;
    }
    
    /// Construct from type and value.
    Variant(const String& type, const String& value) :
        type(VAR_NONE)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(VariantType type, const String& value) :
        type(VAR_NONE)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(const char* type, const char* value) :
        type(VAR_NONE)
    {
        FromString(type, value);
    }

    /// Construct from type and value.
    Variant(VariantType type, const char* value) :
        type(VAR_NONE)
    {
        FromString(type, value);
    }

    /// Destruct.
    ~Variant()
    {
        SetType(VAR_NONE);
    }

    /// Reset to empty.
    void Clear()
    {
        SetType(VAR_NONE);
    }

    /// Assign from another variant.
    Variant& operator = (const Variant& rhs);

    /// Assign from an integer.
    Variant& operator = (int rhs)
    {
        SetType(VAR_INT);
        value.intValue = rhs;
        return *this;
    }

    /// Assign from an unsigned integer.
    Variant& operator = (unsigned rhs)
    {
        SetType(VAR_INT);
        value.intValue = (int)rhs;
        return *this;
    }

    /// Assign from a StringHash (convert to integer.)
    Variant& operator = (const StringHash& rhs)
    {
        SetType(VAR_INT);
        value.intValue = (int)rhs.Value();
        return *this;
    }

    /// Assign from a bool.
    Variant& operator = (bool rhs)
    {
        SetType(VAR_BOOL);
        value.boolValue = rhs;
        return *this;
    }

    /// Assign from a float.
    Variant& operator = (float rhs)
    {
        SetType(VAR_FLOAT);
        value.floatValue = rhs;
        return *this;
    }

    /// Assign from a Vector2.
    Variant& operator = (const Vector2& rhs)
    {
        SetType(VAR_VECTOR2);
        *(reinterpret_cast<Vector2*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a Vector3.
    Variant& operator = (const Vector3& rhs)
    {
        SetType(VAR_VECTOR3);
        *(reinterpret_cast<Vector3*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a Vector4.
    Variant& operator = (const Vector4& rhs)
    {
        SetType(VAR_VECTOR4);
        *(reinterpret_cast<Vector4*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a quaternion.
    Variant& operator = (const Quaternion& rhs)
    {
        SetType(VAR_QUATERNION);
        *(reinterpret_cast<Quaternion*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a color.
    Variant& operator = (const Color& rhs)
    {
        SetType(VAR_COLOR);
        *(reinterpret_cast<Color*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a string.
    Variant& operator = (const String& rhs)
    {
        SetType(VAR_STRING);
        *(reinterpret_cast<String*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a C string.
    Variant& operator = (const char* rhs)
    {
        SetType(VAR_STRING);
        *(reinterpret_cast<String*>(&value)) = String(rhs);
        return *this;
    }

    /// Assign from a buffer.
    Variant& operator = (const Vector<unsigned char>& rhs)
    {
        SetType(VAR_BUFFER);
        *(reinterpret_cast<Vector<unsigned char>*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a void pointer.
    Variant& operator = (void* rhs)
    {
        SetType(VAR_VOIDPTR);
        value.ptrValue = rhs;
        return *this;
    }

    /// Assign from a resource reference.
    Variant& operator = (const ResourceRef& rhs)
    {
        SetType(VAR_RESOURCEREF);
        *(reinterpret_cast<ResourceRef*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a resource reference list.
    Variant& operator = (const ResourceRefList& rhs)
    {
        SetType(VAR_RESOURCEREFLIST);
        *(reinterpret_cast<ResourceRefList*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a variant vector.
    Variant& operator = (const VariantVector& rhs)
    {
        SetType(VAR_VARIANTVECTOR);
        *(reinterpret_cast<VariantVector*>(&value)) = rhs;
        return *this;
    }

    /// Assign from a variant map.
    Variant& operator = (const VariantMap& rhs)
    {
        SetType(VAR_VARIANTMAP);
        *(reinterpret_cast<VariantMap*>(&value)) = rhs;
        return *this;
    }

    /// Assign from an integer rect.
    Variant& operator = (const IntRect& rhs)
    {
        SetType(VAR_INTRECT);
        *(reinterpret_cast<IntRect*>(&value)) = rhs;
        return *this;
    }

    /// Assign from an IntVector2.
    Variant& operator = (const IntVector2& rhs)
    {
        SetType(VAR_INTVECTOR2);
        *(reinterpret_cast<IntVector2*>(&value)) = rhs;
        return *this;
    }
    
    /// Assign from a WeakRefCounted pointer. The object will be stored internally in a WeakPtr so that its expiration can be detected safely.
    Variant& operator = (WeakRefCounted* rhs)
    {
        SetType(VAR_PTR);
        *(reinterpret_cast<WeakPtr<WeakRefCounted>*>(&value)) = rhs;
        return *this;
    }
    
    /// Assign from a Matrix3.
    Variant& operator = (const Matrix3& rhs)
    {
        SetType(VAR_MATRIX3);
        *(reinterpret_cast<Matrix3*>(value.ptrValue)) = rhs;
        return *this;
    }
    
    /// Assign from a Matrix3x4.
    Variant& operator = (const Matrix3x4& rhs)
    {
        SetType(VAR_MATRIX3X4);
        *(reinterpret_cast<Matrix3x4*>(value.ptrValue)) = rhs;
        return *this;
    }
    
    /// Assign from a Matrix4.
    Variant& operator = (const Matrix4& rhs)
    {
        SetType(VAR_MATRIX4);
        *(reinterpret_cast<Matrix4*>(value.ptrValue)) = rhs;
        return *this;
    }
    
    /// Test for equality with another variant.
    bool operator == (const Variant& rhs) const;
    /// Test for equality with an integer. To return true, both the type and value must match.
    bool operator == (int rhs) const { return type == VAR_INT ? value.intValue == rhs : false; }
    /// Test for equality with an unsigned integer. To return true, both the type and value must match.
    bool operator == (unsigned rhs) const { return type == VAR_INT ? value.intValue == (int)rhs : false; }
    /// Test for equality with a bool. To return true, both the type and value must match.
    bool operator == (bool rhs) const { return type == VAR_BOOL ? value.boolValue == rhs : false; }
    /// Test for equality with a float. To return true, both the type and value must match.
    bool operator == (float rhs) const { return type == VAR_FLOAT ? value.floatValue == rhs : false; }
    /// Test for equality with a Vector2. To return true, both the type and value must match.
    bool operator == (const Vector2& rhs) const { return type == VAR_VECTOR2 ? *(reinterpret_cast<const Vector2*>(&value)) == rhs : false; }
    /// Test for equality with a Vector3. To return true, both the type and value must match.
    bool operator == (const Vector3& rhs) const { return type == VAR_VECTOR3 ? *(reinterpret_cast<const Vector3*>(&value)) == rhs : false; }
    /// Test for equality with a Vector4. To return true, both the type and value must match.
    bool operator == (const Vector4& rhs) const { return type == VAR_VECTOR4 ? *(reinterpret_cast<const Vector4*>(&value)) == rhs : false; }
    /// Test for equality with a quaternion. To return true, both the type and value must match.
    bool operator == (const Quaternion& rhs) const { return type == VAR_QUATERNION ? *(reinterpret_cast<const Quaternion*>(&value)) == rhs : false; }
    /// Test for equality with a color. To return true, both the type and value must match.
    bool operator == (const Color& rhs) const { return type == VAR_COLOR ? *(reinterpret_cast<const Color*>(&value)) == rhs : false; }
    /// Test for equality with a string. To return true, both the type and value must match.
    bool operator == (const String& rhs) const { return type == VAR_STRING ? *(reinterpret_cast<const String*>(&value)) == rhs : false; }
    /// Test for equality with a buffer. To return true, both the type and value must match.
    bool operator == (const Vector<unsigned char>& rhs) const { return type == VAR_BUFFER ? *(reinterpret_cast<const Vector<unsigned char>*>(&value)) == rhs : false; }
    
    /// Test for equality with a void pointer. To return true, both the type and value must match, with the exception that a RefCounted pointer is also allowed.
    bool operator == (void* rhs) const
    {
        if (type == VAR_VOIDPTR)
            return value.ptrValue == rhs;
        else if (type == VAR_PTR)
            return *(reinterpret_cast<const WeakPtr<WeakRefCounted>*>(&value)) == rhs;
        else
            return false;
    }
    
    /// Test for equality with a resource reference. To return true, both the type and value must match.
    bool operator == (const ResourceRef& rhs) const { return type == VAR_RESOURCEREF ? *(reinterpret_cast<const ResourceRef*>(&value)) == rhs : false; }
    /// Test for equality with a resource reference list. To return true, both the type and value must match.
    bool operator == (const ResourceRefList& rhs) const { return type == VAR_RESOURCEREFLIST ? *(reinterpret_cast<const ResourceRefList*>(&value)) == rhs : false; }
    /// Test for equality with a variant vector. To return true, both the type and value must match.
    bool operator == (const VariantVector& rhs) const { return type == VAR_VARIANTVECTOR ? *(reinterpret_cast<const VariantVector*>(&value)) == rhs : false; }
    /// Test for equality with a variant map. To return true, both the type and value must match.
    bool operator == (const VariantMap& rhs) const { return type == VAR_VARIANTMAP ? *(reinterpret_cast<const VariantMap*>(&value)) == rhs : false; }
    /// Test for equality with an integer rect. To return true, both the type and value must match.
    bool operator == (const IntRect& rhs) const { return type == VAR_INTRECT ? *(reinterpret_cast<const IntRect*>(&value)) == rhs : false; }
    /// Test for equality with an IntVector2. To return true, both the type and value must match.
    bool operator == (const IntVector2& rhs) const { return type == VAR_INTVECTOR2 ? *(reinterpret_cast<const IntVector2*>(&value)) == rhs : false; }
    /// Test for equality with a StringHash. To return true, both the type and value must match.
    bool operator == (const StringHash& rhs) const { return type == VAR_INT ? (unsigned)value.intValue == rhs.Value() : false; }
    
    /// Test for equality with a WeakRefCounted pointer. To return true, both the type and value must match, with the exception that void pointer is also allowed.
    bool operator == (WeakRefCounted* rhs) const
    {
        if (type == VAR_PTR)
            return *(reinterpret_cast<const WeakPtr<WeakRefCounted>*>(&value)) == rhs;
        else if (type == VAR_VOIDPTR)
            return value.ptrValue == rhs;
        else
            return false;
    }
    
    /// Test for equality with a Matrix3. To return true, both the type and value must match.
    bool operator == (const Matrix3& rhs) const { return type == VAR_MATRIX3 ? *(reinterpret_cast<const Matrix3*>(value.ptrValue)) == rhs : false; }
    /// Test for equality with a Matrix3x4. To return true, both the type and value must match.
    bool operator == (const Matrix3x4& rhs) const { return type == VAR_MATRIX3X4 ? *(reinterpret_cast<const Matrix3x4*>(value.ptrValue)) == rhs : false; }
    /// Test for equality with a Matrix4. To return true, both the type and value must match.
    bool operator == (const Matrix4& rhs) const { return type == VAR_MATRIX4 ? *(reinterpret_cast<const Matrix4*>(value.ptrValue)) == rhs : false; }
    
    /// Test for inequality with another variant.
    bool operator != (const Variant& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an integer.
    bool operator != (int rhs) const { return !(*this == rhs); }
    /// Test for inequality with an unsigned integer.
    bool operator != (unsigned rhs) const { return !(*this == rhs); }
    /// Test for inequality with a bool.
    bool operator != (bool rhs) const { return !(*this == rhs); }
    /// Test for inequality with a float.
    bool operator != (float rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Vector2.
    bool operator != (const Vector2& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Vector3.
    bool operator != (const Vector3& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an Vector4.
    bool operator != (const Vector4& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Quaternion.
    bool operator != (const Quaternion& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a string.
    bool operator != (const String& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a buffer.
    bool operator != (const Vector<unsigned char>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a pointer.
    bool operator != (void* rhs) const { return !(*this == rhs); }
    /// Test for inequality with a resource reference.
    bool operator != (const ResourceRef& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a resource reference list.
    bool operator != (const ResourceRefList& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a variant vector.
    bool operator != (const VariantVector& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a variant map.
    bool operator != (const VariantMap& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an integer rect.
    bool operator != (const IntRect& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an IntVector2.
    bool operator != (const IntVector2& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a StringHash.
    bool operator != (const StringHash& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a WeakRefCounted pointer.
    bool operator != (WeakRefCounted* rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Matrix3.
    bool operator != (const Matrix3& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Matrix3x4.
    bool operator != (const Matrix3x4& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a Matrix4.
    bool operator != (const Matrix4& rhs) const { return !(*this == rhs); }
    
    /// Set from typename and value strings. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(const String& type, const String& value);
    /// Set from typename and value strings. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(const char* type, const char* value);
    /// Set from type and value string. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(VariantType type, const String& value);
    /// Set from type and value string. Pointers will be set to null, and VariantBuffer or VariantMap types are not supported.
    void FromString(VariantType type, const char* value);
    /// Set as a buffer from a memory area.
    void SetBuffer(const void* data, size_t size);

    /// Return int or zero on type mismatch.
    int GetInt() const { return type == VAR_INT ? value.intValue : 0; }
    /// Return unsigned int or zero on type mismatch.
    int GetUInt() const { return type == VAR_INT ? (unsigned)value.intValue : 0; }
    /// Return StringHash or zero on type mismatch.
    StringHash GetStringHash() const { return StringHash(GetUInt()); }
    /// Return bool or false on type mismatch.
    bool GetBool() const { return type == VAR_BOOL ? value.boolValue : false; }
    /// Return float or zero on type mismatch.
    float GetFloat() const { return type == VAR_FLOAT ? value.floatValue : 0.0f; }
    /// Return Vector2 or zero on type mismatch.
    const Vector2& GetVector2() const { return type == VAR_VECTOR2 ? *reinterpret_cast<const Vector2*>(&value) : Vector2::ZERO; }
    /// Return Vector3 or zero on type mismatch.
    const Vector3& GetVector3() const { return type == VAR_VECTOR3 ? *reinterpret_cast<const Vector3*>(&value) : Vector3::ZERO; }
    /// Return Vector4 or zero on type mismatch.
    const Vector4& GetVector4() const { return type == VAR_VECTOR4 ? *reinterpret_cast<const Vector4*>(&value) : Vector4::ZERO; }
    /// Return quaternion or identity on type mismatch.
    const Quaternion& GetQuaternion() const { return type == VAR_QUATERNION ? *reinterpret_cast<const Quaternion*>(&value) : Quaternion::IDENTITY; }
    /// Return color or default on type mismatch.
    const Color& GetColor() const { return type == VAR_COLOR ? *reinterpret_cast<const Color*>(&value) : Color::WHITE; }
    /// Return string or empty on type mismatch.
    const String& GetString() const { return type == VAR_STRING ? *reinterpret_cast<const String*>(&value) : String::EMPTY; }
    /// Return buffer or empty on type mismatch.
    const Vector<unsigned char>& GetBuffer() const { return type == VAR_BUFFER ? *reinterpret_cast<const Vector<unsigned char>*>(&value) : emptyBuffer; }
    
    /// Return void pointer or null on type mismatch. RefCounted pointer will be converted.
    void* GetVoidPtr() const
    {
        if (type == VAR_VOIDPTR)
            return value.ptrValue;
        else if (type == VAR_PTR)
            return *reinterpret_cast<const WeakPtr<WeakRefCounted>*>(&value);
        else
            return 0;
    }
    
    /// Return a resource reference or empty on type mismatch.
    const ResourceRef& GetResourceRef() const { return type == VAR_RESOURCEREF ? *reinterpret_cast<const ResourceRef*>(&value) : emptyResourceRef; }
    /// Return a resource reference list or empty on type mismatch.
    const ResourceRefList& GetResourceRefList() const { return type == VAR_RESOURCEREFLIST ? *reinterpret_cast<const ResourceRefList*>(&value) : emptyResourceRefList; }
    /// Return a variant vector or empty on type mismatch.
    const VariantVector& GetVariantVector() const { return type == VAR_VARIANTVECTOR ? *reinterpret_cast<const VariantVector*>(&value) : emptyVariantVector; }
    /// Return a variant map or empty on type mismatch.
    const VariantMap& GetVariantMap() const { return type == VAR_VARIANTMAP ? *reinterpret_cast<const VariantMap*>(&value) : emptyVariantMap; }
    /// Return an integer rect or empty on type mismatch.
    const IntRect& GetIntRect() const { return type == VAR_INTRECT ? *reinterpret_cast<const IntRect*>(&value) : IntRect::ZERO; }
    /// Return an IntVector2 or empty on type mismatch.
    const IntVector2& GetIntVector2() const { return type == VAR_INTVECTOR2 ? *reinterpret_cast<const IntVector2*>(&value) : IntVector2::ZERO; }
    /// Return a WeakRefCounted pointer or null on type mismatch. Will return null if holding a void pointer, as it can not be safely verified that the object is a WeakRefCounted.
    WeakRefCounted* GetPtr() const { return type == VAR_PTR ? reinterpret_cast<const WeakPtr<WeakRefCounted>*>(&value)->Get() : (WeakRefCounted*)0; }
    /// Return a Matrix3 or identity on type mismatch.
    const Matrix3& GetMatrix3() const { return type == VAR_MATRIX3 ? *(reinterpret_cast<const Matrix3*>(value.ptrValue)) : Matrix3::IDENTITY; }
    /// Return a Matrix3x4 or identity on type mismatch.
    const Matrix3x4& GetMatrix3x4() const { return type == VAR_MATRIX3X4 ? *(reinterpret_cast<const Matrix3x4*>(value.ptrValue)) : Matrix3x4::IDENTITY; }
    /// Return a Matrix4 or identity on type mismatch.
    const Matrix4& GetMatrix4() const { return type == VAR_MATRIX4 ? *(reinterpret_cast<const Matrix4*>(value.ptrValue)) : Matrix4::IDENTITY; }
    /// Return value's type.
    VariantType Type() const { return type; }
    /// Return value's type name.
    String TypeName() const;
    /// Convert value to string. Pointers are returned as null, and VariantBuffer or VariantMap are not supported and return empty.
    String ToString() const;
    /// Return true when the variant value is considered zero according to its actual type.
    bool IsZero() const;
    /// Return true when the variant is empty (i.e. not initialized yet).
    bool IsEmpty() const { return type == VAR_NONE; }
    /// Return the value, template version.
    template <class T> T Get() const;
    
    /// Return a pointer to a modifiable buffer or null on type mismatch.
    Vector<unsigned char>* GetBufferPtr() { return type == VAR_BUFFER ? reinterpret_cast<Vector<unsigned char>*>(&value) : 0; }
    /// Return a pointer to a modifiable variant vector or null on type mismatch.
    VariantVector* GetVariantVectorPtr() { return type == VAR_VARIANTVECTOR ? reinterpret_cast<VariantVector*>(&value) : 0; }
    /// Return a pointer to a modifiable variant map or null on type mismatch.
    VariantMap* GetVariantMapPtr() { return type == VAR_VARIANTMAP ? reinterpret_cast<VariantMap*>(&value) : 0; }

    /// Return name for variant type.
    static String TypeName(VariantType type);
    /// Return variant type from type name.
    static VariantType TypeFromName(const String& typeName);
    /// Return variant type from type name.
    static VariantType TypeFromName(const char* typeName);

    /// Empty variant.
    static const Variant EMPTY;
    /// Empty buffer.
    static const Vector<unsigned char> emptyBuffer;
    /// Empty resource reference.
    static const ResourceRef emptyResourceRef;
    /// Empty resource reference list.
    static const ResourceRefList emptyResourceRefList;
    /// Empty variant map.
    static const VariantMap emptyVariantMap;
    /// Empty variant vector.
    static const VariantVector emptyVariantVector;

private:
    /// Set new type and allocate/deallocate memory as necessary.
    void SetType(VariantType newType);

    /// Variant type.
    VariantType type;
    /// Variant value.
    VariantValue value;
};

}
