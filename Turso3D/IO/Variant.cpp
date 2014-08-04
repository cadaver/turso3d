// For conditions of distribution and use, see copyright notice in License.txt

#include "Variant.h"

#include <cstring>

namespace Turso3D
{

const Variant Variant::EMPTY;
const Vector<unsigned char> Variant::emptyBuffer;
const ResourceRef Variant::emptyResourceRef;
const ResourceRefList Variant::emptyResourceRefList;
const VariantMap Variant::emptyVariantMap;
const VariantVector Variant::emptyVariantVector;

static const char* typeNames[] =
{
    "None",
    "Int",
    "Bool",
    "Float",
    "Vector2",
    "Vector3",
    "Vector4",
    "Quaternion",
    "Color",
    "String",
    "Buffer",
    "VoidPtr",
    "ResourceRef",
    "ResourceRefList",
    "VariantVector",
    "VariantMap",
    "IntRect",
    "IntVector2",
    "Ptr",
    "Matrix3",
    "Matrix3x4",
    "Matrix4",
    0
};

Variant& Variant::operator = (const Variant& rhs)
{
    SetType(rhs.Type());

    switch (type)
    {
    case VAR_STRING:
        *(reinterpret_cast<String*>(&value)) = *(reinterpret_cast<const String*>(&rhs.value));
        break;

    case VAR_BUFFER:
        *(reinterpret_cast<Vector<unsigned char>*>(&value)) = *(reinterpret_cast<const Vector<unsigned char>*>(&rhs.value));
        break;

    case VAR_RESOURCEREF:
        *(reinterpret_cast<ResourceRef*>(&value)) = *(reinterpret_cast<const ResourceRef*>(&rhs.value));
        break;

    case VAR_RESOURCEREFLIST:
        *(reinterpret_cast<ResourceRefList*>(&value)) = *(reinterpret_cast<const ResourceRefList*>(&rhs.value));
        break;

    case VAR_VARIANTVECTOR:
        *(reinterpret_cast<VariantVector*>(&value)) = *(reinterpret_cast<const VariantVector*>(&rhs.value));
        break;

    case VAR_VARIANTMAP:
        *(reinterpret_cast<VariantMap*>(&value)) = *(reinterpret_cast<const VariantMap*>(&rhs.value));
        break;

    case VAR_PTR:
        *(reinterpret_cast<WeakPtr<WeakRefCounted>*>(&value)) = *(reinterpret_cast<const WeakPtr<WeakRefCounted>*>(&rhs.value));
        break;
        
    case VAR_MATRIX3:
        *(reinterpret_cast<Matrix3*>(value.ptrValue)) = *(reinterpret_cast<const Matrix3*>(rhs.value.ptrValue));
        break;
        
    case VAR_MATRIX3X4:
        *(reinterpret_cast<Matrix3x4*>(value.ptrValue)) = *(reinterpret_cast<const Matrix3x4*>(rhs.value.ptrValue));
        break;
        
    case VAR_MATRIX4:
        *(reinterpret_cast<Matrix4*>(value.ptrValue)) = *(reinterpret_cast<const Matrix4*>(rhs.value.ptrValue));
        break;
        
    default:
        value = rhs.value;
        break;
    }

    return *this;
}

bool Variant::operator == (const Variant& rhs) const
{
    if (type == VAR_VOIDPTR || type == VAR_PTR)
        return GetVoidPtr() == rhs.GetVoidPtr();
    else if (type != rhs.type)
        return false;
    
    switch (type)
    {
    case VAR_INT:
        return value.intValue == rhs.value.intValue;

    case VAR_BOOL:
        return value.boolValue == rhs.value.boolValue;

    case VAR_FLOAT:
        return value.floatValue == rhs.value.floatValue;

    case VAR_VECTOR2:
        return *(reinterpret_cast<const Vector2*>(&value)) == *(reinterpret_cast<const Vector2*>(&rhs.value));

    case VAR_VECTOR3:
        return *(reinterpret_cast<const Vector3*>(&value)) == *(reinterpret_cast<const Vector3*>(&rhs.value));

    case VAR_VECTOR4:
    case VAR_QUATERNION:
    case VAR_COLOR:
        // Hack: use the Vector4 compare for all these classes, as they have the same memory structure
        return *(reinterpret_cast<const Vector4*>(&value)) == *(reinterpret_cast<const Vector4*>(&rhs.value));

    case VAR_STRING:
        return *(reinterpret_cast<const String*>(&value)) == *(reinterpret_cast<const String*>(&rhs.value));

    case VAR_BUFFER:
        return *(reinterpret_cast<const Vector<unsigned char>*>(&value)) == *(reinterpret_cast<const Vector<unsigned char>*>(&rhs.value));

    case VAR_RESOURCEREF:
        return *(reinterpret_cast<const ResourceRef*>(&value)) == *(reinterpret_cast<const ResourceRef*>(&rhs.value));

    case VAR_RESOURCEREFLIST:
        return *(reinterpret_cast<const ResourceRefList*>(&value)) == *(reinterpret_cast<const ResourceRefList*>(&rhs.value));

    case VAR_VARIANTVECTOR:
        return *(reinterpret_cast<const VariantVector*>(&value)) == *(reinterpret_cast<const VariantVector*>(&rhs.value));

    case VAR_VARIANTMAP:
        return *(reinterpret_cast<const VariantMap*>(&value)) == *(reinterpret_cast<const VariantMap*>(&rhs.value));

    case VAR_INTRECT:
        return *(reinterpret_cast<const IntRect*>(&value)) == *(reinterpret_cast<const IntRect*>(&rhs.value));

    case VAR_INTVECTOR2:
        return *(reinterpret_cast<const IntVector2*>(&value)) == *(reinterpret_cast<const IntVector2*>(&rhs.value));

    case VAR_MATRIX3:
        return *(reinterpret_cast<const Matrix3*>(value.ptrValue)) == *(reinterpret_cast<const Matrix3*>(rhs.value.ptrValue));

    case VAR_MATRIX3X4:
        return *(reinterpret_cast<const Matrix3x4*>(value.ptrValue)) == *(reinterpret_cast<const Matrix3x4*>(rhs.value.ptrValue));

    case VAR_MATRIX4:
        return *(reinterpret_cast<const Matrix4*>(value.ptrValue)) == *(reinterpret_cast<const Matrix4*>(rhs.value.ptrValue));

    default:
        return true;
    }
}

void Variant::FromString(const String& type_, const String& value_)
{
    return FromString(TypeFromName(type_), value_.CString());
}

void Variant::FromString(const char* type_, const char* value_)
{
    return FromString(TypeFromName(type_), value_);
}

void Variant::FromString(VariantType type_, const String& value_)
{
    return FromString(type_, value_.CString());
}

void Variant::FromString(VariantType type_, const char* value_)
{
    switch (type_)
    {
    case VAR_INT:
        *this = String::ToInt(value_);
        break;

    case VAR_BOOL:
        *this = String::ToBool(value_);
        break;

    case VAR_FLOAT:
        *this = String::ToFloat(value_);
        break;

    case VAR_VECTOR2:
        *this = Vector2(value_);
        break;

    case VAR_VECTOR3:
        *this = Vector3(value_);
        break;

    case VAR_VECTOR4:
        *this = Vector4(value_);
        break;

    case VAR_QUATERNION:
        *this = Quaternion(value_);
        break;

    case VAR_COLOR:
        *this = Color(value_);
        break;

    case VAR_STRING:
        *this = value_;
        break;

    case VAR_BUFFER:
        {
            SetType(VAR_BUFFER);
            Vector<unsigned char>& buffer = *(reinterpret_cast<Vector<unsigned char>*>(&value));
            StringToBuffer(buffer, value_);
        }
        break;

    case VAR_VOIDPTR:
        // From string to void pointer not supported, set to null
        *this = (void*)0;
        break;

    case VAR_RESOURCEREF:
        {
            Vector<String> values = String::Split(value_, ';');
            if (values.Size() == 2)
            {
                SetType(VAR_RESOURCEREF);
                ResourceRef& ref = *(reinterpret_cast<ResourceRef*>(&value));
                ref.type = values[0];
                ref.name = values[1];
            }
        }
        break;

    case VAR_RESOURCEREFLIST:
        {
            Vector<String> values = String::Split(value_, ';');
            if (values.Size() >= 1)
            {
                SetType(VAR_RESOURCEREFLIST);
                ResourceRefList& refList = *(reinterpret_cast<ResourceRefList*>(&value));
                refList.type = values[0];
                refList.names.Resize(values.Size() - 1);
                for (size_t i = 1; i < values.Size(); ++i)
                    refList.names[i - 1] = values[i];
            }
        }
        break;

    case VAR_INTRECT:
        *this = IntRect(value_);
        break;

    case VAR_INTVECTOR2:
        *this = IntVector2(value_);
        break;

    case VAR_PTR:
        // From string to WeakRefCounted pointer not supported, set to null
        *this = (WeakRefCounted*)0;
        break;
        
    case VAR_MATRIX3:
        *this = Matrix3(value_);
        break;
        
    case VAR_MATRIX3X4:
        *this = Matrix3x4(value_);
        break;
        
    case VAR_MATRIX4:
        *this = Matrix4(value_);
        break;
        
    default:
        SetType(VAR_NONE);
    }
}

void Variant::SetBuffer(const void* data, size_t size)
{
    if (size && !data)
        size = 0;

    SetType(VAR_BUFFER);
    Vector<unsigned char>& buffer = *(reinterpret_cast<Vector<unsigned char>*>(&value));
    buffer.Resize(size);
    if (size)
        memcpy(&buffer[0], data, size);
}

String Variant::TypeName() const
{
    return typeNames[type];
}

String Variant::ToString() const
{
    switch (type)
    {
    case VAR_INT:
        return String(value.intValue);

    case VAR_BOOL:
        return String(value.boolValue);

    case VAR_FLOAT:
        return String(value.floatValue);

    case VAR_VECTOR2:
        return (reinterpret_cast<const Vector2*>(&value))->ToString();

    case VAR_VECTOR3:
        return (reinterpret_cast<const Vector3*>(&value))->ToString();

    case VAR_VECTOR4:
        return (reinterpret_cast<const Vector4*>(&value))->ToString();

    case VAR_QUATERNION:
        return (reinterpret_cast<const Quaternion*>(&value))->ToString();

    case VAR_COLOR:
        return (reinterpret_cast<const Color*>(&value))->ToString();

    case VAR_STRING:
        return *(reinterpret_cast<const String*>(&value));

    case VAR_BUFFER:
        {
            const Vector<unsigned char>& buffer = *(reinterpret_cast<const Vector<unsigned char>*>(&value));
            String ret;
            BufferToString(ret, buffer.Begin().ptr, buffer.Size());
            return ret;
        }

    case VAR_VOIDPTR:
    case VAR_PTR:
        // Pointer serialization not supported (convert to null)
        return String(0);

    case VAR_INTRECT:
        return (reinterpret_cast<const IntRect*>(&value))->ToString();

    case VAR_INTVECTOR2:
        return (reinterpret_cast<const IntVector2*>(&value))->ToString();

    default:
        // VAR_RESOURCEREF, VAR_RESOURCEREFLIST, VAR_VARIANTVECTOR, VAR_VARIANTMAP
        // Reference string serialization requires typehash-to-name mapping from the context. Can not support here
        // Also variant map or vector string serialization is not supported. XML or binary save should be used instead
        return String::EMPTY;

    case VAR_MATRIX3:
        return (reinterpret_cast<const Matrix3*>(value.ptrValue))->ToString();

    case VAR_MATRIX3X4:
        return (reinterpret_cast<const Matrix3x4*>(value.ptrValue))->ToString();
        
    case VAR_MATRIX4:
        return (reinterpret_cast<const Matrix4*>(value.ptrValue))->ToString();
    }
}

bool Variant::IsZero() const
{
    switch (type)
    {
    case VAR_INT:
        return value.intValue == 0;

    case VAR_BOOL:
        return value.boolValue == false;

    case VAR_FLOAT:
        return value.floatValue == 0.0f;

    case VAR_VECTOR2:
        return *reinterpret_cast<const Vector2*>(&value) == Vector2::ZERO;

    case VAR_VECTOR3:
        return *reinterpret_cast<const Vector3*>(&value) == Vector3::ZERO;

    case VAR_VECTOR4:
        return *reinterpret_cast<const Vector4*>(&value) == Vector4::ZERO;

    case VAR_QUATERNION:
        return *reinterpret_cast<const Quaternion*>(&value) == Quaternion::IDENTITY;

    case VAR_COLOR:
        // WHITE is considered empty (i.e. default) color in the Color class definition
        return *reinterpret_cast<const Color*>(&value) == Color::WHITE;

    case VAR_STRING:
        return reinterpret_cast<const String*>(&value)->IsEmpty();

    case VAR_BUFFER:
        return reinterpret_cast<const Vector<unsigned char>*>(&value)->IsEmpty();

    case VAR_VOIDPTR:
        return value.ptrValue == 0;

    case VAR_RESOURCEREF:
        return reinterpret_cast<const ResourceRef*>(&value)->name.IsEmpty();

    case VAR_RESOURCEREFLIST:
    {
        const Vector<String>& names = reinterpret_cast<const ResourceRefList*>(&value)->names;
        for (Vector<String>::ConstIterator i = names.Begin(); i != names.End(); ++i)
        {
            if (!i->IsEmpty())
                return false;
        }
        return true;
    }

    case VAR_VARIANTVECTOR:
        return reinterpret_cast<const VariantVector*>(&value)->IsEmpty();

    case VAR_VARIANTMAP:
        return reinterpret_cast<const VariantMap*>(&value)->IsEmpty();

    case VAR_INTRECT:
        return *reinterpret_cast<const IntRect*>(&value) == IntRect::ZERO;

    case VAR_INTVECTOR2:
        return *reinterpret_cast<const IntVector2*>(&value) == IntVector2::ZERO;

    case VAR_PTR:
        return reinterpret_cast<const WeakPtr<WeakRefCounted>*>(&value)->IsNull();
        
    case VAR_MATRIX3:
        return *reinterpret_cast<const Matrix3*>(value.ptrValue) == Matrix3::IDENTITY;
        
    case VAR_MATRIX3X4:
        return *reinterpret_cast<const Matrix3x4*>(value.ptrValue) == Matrix3x4::IDENTITY;
        
    case VAR_MATRIX4:
        return *reinterpret_cast<const Matrix4*>(value.ptrValue) == Matrix4::IDENTITY;
        
    default:
        return true;
    }
}

void Variant::SetType(VariantType newType)
{
    if (type == newType)
        return;

    switch (type)
    {
    case VAR_STRING:
        (reinterpret_cast<String*>(&value))->~String();
        break;

    case VAR_BUFFER:
        (reinterpret_cast<Vector<unsigned char>*>(&value))->~Vector<unsigned char>();
        break;

    case VAR_RESOURCEREF:
        (reinterpret_cast<ResourceRef*>(&value))->~ResourceRef();
        break;

    case VAR_RESOURCEREFLIST:
        (reinterpret_cast<ResourceRefList*>(&value))->~ResourceRefList();
        break;

    case VAR_VARIANTVECTOR:
        (reinterpret_cast<VariantVector*>(&value))->~VariantVector();
        break;

    case VAR_VARIANTMAP:
        (reinterpret_cast<VariantMap*>(&value))->~VariantMap();
        break;

    case VAR_PTR:
        (reinterpret_cast<WeakPtr<WeakRefCounted>*>(&value))->~WeakPtr<WeakRefCounted>();
        break;
        
    case VAR_MATRIX3:
        delete reinterpret_cast<Matrix3*>(value.ptrValue);
        break;
        
    case VAR_MATRIX3X4:
        delete reinterpret_cast<Matrix3x4*>(value.ptrValue);
        break;
        
    case VAR_MATRIX4:
        delete reinterpret_cast<Matrix4*>(value.ptrValue);
        break;
        
    default:
        break;
    }

    type = newType;

    switch (type)
    {
    case VAR_STRING:
        new(reinterpret_cast<String*>(&value)) String();
        break;

    case VAR_BUFFER:
        new(reinterpret_cast<Vector<unsigned char>*>(&value)) Vector<unsigned char>();
        break;

    case VAR_RESOURCEREF:
        new(reinterpret_cast<ResourceRef*>(&value)) ResourceRef();
        break;

    case VAR_RESOURCEREFLIST:
        new(reinterpret_cast<ResourceRefList*>(&value)) ResourceRefList();
        break;

    case VAR_VARIANTVECTOR:
        new(reinterpret_cast<VariantVector*>(&value)) VariantVector();
        break;

    case VAR_VARIANTMAP:
        new(reinterpret_cast<VariantMap*>(&value)) VariantMap();
        break;

    case VAR_PTR:
        new(reinterpret_cast<WeakPtr<WeakRefCounted>*>(&value)) WeakPtr<WeakRefCounted>();
        break;
        
    case VAR_MATRIX3:
        value.ptrValue = new Matrix3();
        break;
        
    case VAR_MATRIX3X4:
        value.ptrValue = new Matrix3x4();
        break;
        
    case VAR_MATRIX4:
        value.ptrValue = new Matrix4();
        break;
        
    default:
        break;
    }
}

template<> int Variant::Get<int>() const
{
    return GetInt();
}

template<> unsigned Variant::Get<unsigned>() const
{
    return GetUInt();
}

template<> StringHash Variant::Get<StringHash>() const
{
    return GetStringHash();
}

template<> bool Variant::Get<bool>() const
{
    return GetBool();
}

template<> float Variant::Get<float>() const
{
    return GetFloat();
}

template<> const Vector2& Variant::Get<const Vector2&>() const
{
    return GetVector2();
}

template<> const Vector3& Variant::Get<const Vector3&>() const
{
    return GetVector3();
}

template<> const Vector4& Variant::Get<const Vector4&>() const
{
    return GetVector4();
}

template<> const Quaternion& Variant::Get<const Quaternion&>() const
{
    return GetQuaternion();
}

template<> const Color& Variant::Get<const Color&>() const
{
    return GetColor();
}

template<> const String& Variant::Get<const String&>() const
{
    return GetString();
}

template<> const IntRect& Variant::Get<const IntRect&>() const
{
    return GetIntRect();
}

template<> const IntVector2& Variant::Get<const IntVector2&>() const
{
    return GetIntVector2();
}

template<> const Vector<unsigned char>& Variant::Get<const Vector<unsigned char>& >() const
{
    return GetBuffer();
}

template<> void* Variant::Get<void*>() const
{
    return GetVoidPtr();
}

template<> WeakRefCounted* Variant::Get<WeakRefCounted*>() const
{
    return GetPtr();
}

template<> const Matrix3& Variant::Get<const Matrix3&>() const
{
    return GetMatrix3();
}

template<> const Matrix3x4& Variant::Get<const Matrix3x4&>() const
{
    return GetMatrix3x4();
}

template<> const Matrix4& Variant::Get<const Matrix4&>() const
{
    return GetMatrix4();
}

template<> ResourceRef Variant::Get<ResourceRef>() const
{
    return GetResourceRef();
}

template<> ResourceRefList Variant::Get<ResourceRefList>() const
{
    return GetResourceRefList();
}

template<> VariantVector Variant::Get<VariantVector>() const
{
    return GetVariantVector();
}

template<> VariantMap Variant::Get<VariantMap>() const
{
    return GetVariantMap();
}

template<> Vector2 Variant::Get<Vector2>() const
{
    return GetVector2();
}

template<> Vector3 Variant::Get<Vector3>() const
{
    return GetVector3();
}

template<> Vector4 Variant::Get<Vector4>() const
{
    return GetVector4();
}

template<> Quaternion Variant::Get<Quaternion>() const
{
    return GetQuaternion();
}

template<> Color Variant::Get<Color>() const
{
    return GetColor();
}

template<> String Variant::Get<String>() const
{
    return GetString();
}

template<> IntRect Variant::Get<IntRect>() const
{
    return GetIntRect();
}

template<> IntVector2 Variant::Get<IntVector2>() const
{
    return GetIntVector2();
}

template<> Vector<unsigned char> Variant::Get<Vector<unsigned char> >() const
{
    return GetBuffer();
}

template<> Matrix3 Variant::Get<Matrix3>() const
{
    return GetMatrix3();
}

template<> Matrix3x4 Variant::Get<Matrix3x4>() const
{
    return GetMatrix3x4();
}

template<> Matrix4 Variant::Get<Matrix4>() const
{
    return GetMatrix4();
}

String Variant::TypeName(VariantType type)
{
    return typeNames[type];
}

VariantType Variant::TypeFromName(const String& typeName)
{
    return TypeFromName(typeName.CString());
}

VariantType Variant::TypeFromName(const char* typeName)
{
    return (VariantType)StringListIndex(typeName, typeNames, VAR_NONE);
}

}
