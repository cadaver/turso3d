// For conditions of distribution and use, see copyright notice in License.txt

#include "../Object/Object.h"
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

bool ResourceRef::FromString(const String& str)
{
    return FromString(str.CString());
}

bool ResourceRef::FromString(const char* str)
{
    Vector<String> values = String::Split(str, ';');
    if (values.Size() == 2)
    {
        type = values[0];
        name = values[1];
        return true;
    }
    else
        return false;
}

String ResourceRef::ToString() const
{
    return TypeNameFromType(type) + ";" + name;
}

bool ResourceRefList::FromString(const String& str)
{
    return FromString(str.CString());
}

bool ResourceRefList::FromString(const char* str)
{
    Vector<String> values = String::Split(str, ';');
    if (values.Size() >= 1)
    {
        type = values[0];
        names.Clear();
        for (size_t i = 1; i < values.Size(); ++i)
            names.Push(values[i]);
        return true;
    }
    else
        return false;
}

String ResourceRefList::ToString() const
{
    String ret(TypeNameFromType(type));
    for (size_t i = 0; i < names.Size(); ++i)
    {
        ret += ";";
        ret += names[i];
    }
    return ret;
}

Variant& Variant::operator = (const Variant& rhs)
{
    SetType(rhs.Type());

    switch (type)
    {
    case VAR_STRING:
        *(reinterpret_cast<String*>(&data)) = *(reinterpret_cast<const String*>(&rhs.data));
        break;

    case VAR_BUFFER:
        *(reinterpret_cast<Vector<unsigned char>*>(&data)) = *(reinterpret_cast<const Vector<unsigned char>*>(&rhs.data));
        break;

    case VAR_RESOURCEREF:
        *(reinterpret_cast<ResourceRef*>(&data)) = *(reinterpret_cast<const ResourceRef*>(&rhs.data));
        break;

    case VAR_RESOURCEREFLIST:
        *(reinterpret_cast<ResourceRefList*>(&data)) = *(reinterpret_cast<const ResourceRefList*>(&rhs.data));
        break;

    case VAR_VARIANTVECTOR:
        *(reinterpret_cast<VariantVector*>(&data)) = *(reinterpret_cast<const VariantVector*>(&rhs.data));
        break;

    case VAR_VARIANTMAP:
        *(reinterpret_cast<VariantMap*>(&data)) = *(reinterpret_cast<const VariantMap*>(&rhs.data));
        break;

    case VAR_PTR:
        *(reinterpret_cast<WeakPtr<WeakRefCounted>*>(&data)) = *(reinterpret_cast<const WeakPtr<WeakRefCounted>*>(&rhs.data));
        break;
        
    case VAR_MATRIX3:
        *(reinterpret_cast<Matrix3*>(data.ptrValue)) = *(reinterpret_cast<const Matrix3*>(rhs.data.ptrValue));
        break;
        
    case VAR_MATRIX3X4:
        *(reinterpret_cast<Matrix3x4*>(data.ptrValue)) = *(reinterpret_cast<const Matrix3x4*>(rhs.data.ptrValue));
        break;
        
    case VAR_MATRIX4:
        *(reinterpret_cast<Matrix4*>(data.ptrValue)) = *(reinterpret_cast<const Matrix4*>(rhs.data.ptrValue));
        break;
        
    default:
        data = rhs.data;
        break;
    }

    return *this;
}

bool Variant::operator == (const Variant& rhs) const
{
    if (type == VAR_VOIDPTR || type == VAR_PTR)
        return AsVoidPtr() == rhs.AsVoidPtr();
    else if (type != rhs.type)
        return false;
    
    switch (type)
    {
    case VAR_INT:
        return data.intValue == rhs.data.intValue;

    case VAR_BOOL:
        return data.boolValue == rhs.data.boolValue;

    case VAR_FLOAT:
        return data.floatValue == rhs.data.floatValue;

    case VAR_VECTOR2:
        return *(reinterpret_cast<const Vector2*>(&data)) == *(reinterpret_cast<const Vector2*>(&rhs.data));

    case VAR_VECTOR3:
        return *(reinterpret_cast<const Vector3*>(&data)) == *(reinterpret_cast<const Vector3*>(&rhs.data));

    case VAR_VECTOR4:
    case VAR_QUATERNION:
    case VAR_COLOR:
        // Hack: use the Vector4 compare for all these classes, as they have the same memory structure
        return *(reinterpret_cast<const Vector4*>(&data)) == *(reinterpret_cast<const Vector4*>(&rhs.data));

    case VAR_STRING:
        return *(reinterpret_cast<const String*>(&data)) == *(reinterpret_cast<const String*>(&rhs.data));

    case VAR_BUFFER:
        return *(reinterpret_cast<const Vector<unsigned char>*>(&data)) == *(reinterpret_cast<const Vector<unsigned char>*>(&rhs.data));

    case VAR_RESOURCEREF:
        return *(reinterpret_cast<const ResourceRef*>(&data)) == *(reinterpret_cast<const ResourceRef*>(&rhs.data));

    case VAR_RESOURCEREFLIST:
        return *(reinterpret_cast<const ResourceRefList*>(&data)) == *(reinterpret_cast<const ResourceRefList*>(&rhs.data));

    case VAR_VARIANTVECTOR:
        return *(reinterpret_cast<const VariantVector*>(&data)) == *(reinterpret_cast<const VariantVector*>(&rhs.data));

    case VAR_VARIANTMAP:
        return *(reinterpret_cast<const VariantMap*>(&data)) == *(reinterpret_cast<const VariantMap*>(&rhs.data));

    case VAR_INTRECT:
        return *(reinterpret_cast<const IntRect*>(&data)) == *(reinterpret_cast<const IntRect*>(&rhs.data));

    case VAR_INTVECTOR2:
        return *(reinterpret_cast<const IntVector2*>(&data)) == *(reinterpret_cast<const IntVector2*>(&rhs.data));

    case VAR_MATRIX3:
        return *(reinterpret_cast<const Matrix3*>(data.ptrValue)) == *(reinterpret_cast<const Matrix3*>(rhs.data.ptrValue));

    case VAR_MATRIX3X4:
        return *(reinterpret_cast<const Matrix3x4*>(data.ptrValue)) == *(reinterpret_cast<const Matrix3x4*>(rhs.data.ptrValue));

    case VAR_MATRIX4:
        return *(reinterpret_cast<const Matrix4*>(data.ptrValue)) == *(reinterpret_cast<const Matrix4*>(rhs.data.ptrValue));

    default:
        return true;
    }
}

void Variant::FromString(const String& type_, const String& value)
{
    return FromString(TypeFromName(type_), value.CString());
}

void Variant::FromString(const char* type_, const char* value)
{
    return FromString(TypeFromName(type_), value);
}

void Variant::FromString(VariantType type_, const String& value)
{
    return FromString(type_, value.CString());
}

void Variant::FromString(VariantType type_, const char* value)
{
    switch (type_)
    {
    case VAR_INT:
        *this = String::ToInt(value);
        break;

    case VAR_BOOL:
        *this = String::ToBool(value);
        break;

    case VAR_FLOAT:
        *this = String::ToFloat(value);
        break;

    case VAR_VECTOR2:
        *this = Vector2(value);
        break;

    case VAR_VECTOR3:
        *this = Vector3(value);
        break;

    case VAR_VECTOR4:
        *this = Vector4(value);
        break;

    case VAR_QUATERNION:
        *this = Quaternion(value);
        break;

    case VAR_COLOR:
        *this = Color(value);
        break;

    case VAR_STRING:
        *this = value;
        break;

    case VAR_BUFFER:
        {
            SetType(VAR_BUFFER);
            Vector<unsigned char>& buffer = *(reinterpret_cast<Vector<unsigned char>*>(&data));
            StringToBuffer(buffer, value);
        }
        break;

    case VAR_VOIDPTR:
        // From string to void pointer not supported, set to null
        *this = (void*)0;
        break;

    case VAR_RESOURCEREF:
        *this = ResourceRef(value);
        break;
        
    case VAR_RESOURCEREFLIST:
        *this = ResourceRefList(value);
        break;
        
    case VAR_INTRECT:
        *this = IntRect(value);
        break;

    case VAR_INTVECTOR2:
        *this = IntVector2(value);
        break;

    case VAR_PTR:
        // From string to WeakRefCounted pointer not supported, set to null
        *this = (WeakRefCounted*)0;
        break;
        
    case VAR_MATRIX3:
        *this = Matrix3(value);
        break;
        
    case VAR_MATRIX3X4:
        *this = Matrix3x4(value);
        break;
        
    case VAR_MATRIX4:
        *this = Matrix4(value);
        break;
        
    default:
        SetType(VAR_NONE);
    }
}

void Variant::SetBuffer(const void* bytes, size_t size)
{
    if (size && !bytes)
        size = 0;

    SetType(VAR_BUFFER);
    Vector<unsigned char>& buffer = *(reinterpret_cast<Vector<unsigned char>*>(&data));
    buffer.Resize(size);
    if (size)
        memcpy(&buffer[0], bytes, size);
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
        return String(data.intValue);

    case VAR_BOOL:
        return String(data.boolValue);

    case VAR_FLOAT:
        return String(data.floatValue);

    case VAR_VECTOR2:
        return (reinterpret_cast<const Vector2*>(&data))->ToString();

    case VAR_VECTOR3:
        return (reinterpret_cast<const Vector3*>(&data))->ToString();

    case VAR_VECTOR4:
        return (reinterpret_cast<const Vector4*>(&data))->ToString();

    case VAR_QUATERNION:
        return (reinterpret_cast<const Quaternion*>(&data))->ToString();

    case VAR_COLOR:
        return (reinterpret_cast<const Color*>(&data))->ToString();

    case VAR_STRING:
        return *(reinterpret_cast<const String*>(&data));

    case VAR_BUFFER:
        {
            String ret;
            const Vector<unsigned char>& buffer = *(reinterpret_cast<const Vector<unsigned char>*>(&data));
            BufferToString(ret, buffer.Begin().ptr, buffer.Size());
            return ret;
        }

    case VAR_VOIDPTR:
    case VAR_PTR:
        // Pointer serialization not supported (convert to null)
        return String(0);

    case VAR_INTRECT:
        return (reinterpret_cast<const IntRect*>(&data))->ToString();

    case VAR_INTVECTOR2:
        return (reinterpret_cast<const IntVector2*>(&data))->ToString();

    case VAR_RESOURCEREF:
        return (reinterpret_cast<const ResourceRef*>(&data))->ToString();
        
    case VAR_RESOURCEREFLIST:
        return (reinterpret_cast<const ResourceRefList*>(&data))->ToString();
        
    case VAR_MATRIX3:
        return (reinterpret_cast<const Matrix3*>(data.ptrValue))->ToString();

    case VAR_MATRIX3X4:
        return (reinterpret_cast<const Matrix3x4*>(data.ptrValue))->ToString();
        
    case VAR_MATRIX4:
        return (reinterpret_cast<const Matrix4*>(data.ptrValue))->ToString();

    default:
        // VAR_VARIANTVECTOR, VAR_VARIANTMAP
        // Not supported. Binary or JSON save should be used instead.
        return String::EMPTY;
    }
}

bool Variant::IsZero() const
{
    switch (type)
    {
    case VAR_INT:
        return data.intValue == 0;

    case VAR_BOOL:
        return data.boolValue == false;

    case VAR_FLOAT:
        return data.floatValue == 0.0f;

    case VAR_VECTOR2:
        return *reinterpret_cast<const Vector2*>(&data) == Vector2::ZERO;

    case VAR_VECTOR3:
        return *reinterpret_cast<const Vector3*>(&data) == Vector3::ZERO;

    case VAR_VECTOR4:
        return *reinterpret_cast<const Vector4*>(&data) == Vector4::ZERO;

    case VAR_QUATERNION:
        return *reinterpret_cast<const Quaternion*>(&data) == Quaternion::IDENTITY;

    case VAR_COLOR:
        // WHITE is considered empty (i.e. default) color in the Color class definition
        return *reinterpret_cast<const Color*>(&data) == Color::WHITE;

    case VAR_STRING:
        return reinterpret_cast<const String*>(&data)->IsEmpty();

    case VAR_BUFFER:
        return reinterpret_cast<const Vector<unsigned char>*>(&data)->IsEmpty();

    case VAR_VOIDPTR:
        return data.ptrValue == 0;

    case VAR_RESOURCEREF:
        return reinterpret_cast<const ResourceRef*>(&data)->name.IsEmpty();

    case VAR_RESOURCEREFLIST:
    {
        const Vector<String>& names = reinterpret_cast<const ResourceRefList*>(&data)->names;
        for (Vector<String>::ConstIterator i = names.Begin(); i != names.End(); ++i)
        {
            if (!i->IsEmpty())
                return false;
        }
        return true;
    }

    case VAR_VARIANTVECTOR:
        return reinterpret_cast<const VariantVector*>(&data)->IsEmpty();

    case VAR_VARIANTMAP:
        return reinterpret_cast<const VariantMap*>(&data)->IsEmpty();

    case VAR_INTRECT:
        return *reinterpret_cast<const IntRect*>(&data) == IntRect::ZERO;

    case VAR_INTVECTOR2:
        return *reinterpret_cast<const IntVector2*>(&data) == IntVector2::ZERO;

    case VAR_PTR:
        return reinterpret_cast<const WeakPtr<WeakRefCounted>*>(&data)->IsNull();
        
    case VAR_MATRIX3:
        return *reinterpret_cast<const Matrix3*>(data.ptrValue) == Matrix3::IDENTITY;
        
    case VAR_MATRIX3X4:
        return *reinterpret_cast<const Matrix3x4*>(data.ptrValue) == Matrix3x4::IDENTITY;
        
    case VAR_MATRIX4:
        return *reinterpret_cast<const Matrix4*>(data.ptrValue) == Matrix4::IDENTITY;
        
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
        (reinterpret_cast<String*>(&data))->~String();
        break;

    case VAR_BUFFER:
        (reinterpret_cast<Vector<unsigned char>*>(&data))->~Vector<unsigned char>();
        break;

    case VAR_RESOURCEREF:
        (reinterpret_cast<ResourceRef*>(&data))->~ResourceRef();
        break;

    case VAR_RESOURCEREFLIST:
        (reinterpret_cast<ResourceRefList*>(&data))->~ResourceRefList();
        break;

    case VAR_VARIANTVECTOR:
        (reinterpret_cast<VariantVector*>(&data))->~VariantVector();
        break;

    case VAR_VARIANTMAP:
        (reinterpret_cast<VariantMap*>(&data))->~VariantMap();
        break;

    case VAR_PTR:
        (reinterpret_cast<WeakPtr<WeakRefCounted>*>(&data))->~WeakPtr<WeakRefCounted>();
        break;
        
    case VAR_MATRIX3:
        delete reinterpret_cast<Matrix3*>(data.ptrValue);
        break;
        
    case VAR_MATRIX3X4:
        delete reinterpret_cast<Matrix3x4*>(data.ptrValue);
        break;
        
    case VAR_MATRIX4:
        delete reinterpret_cast<Matrix4*>(data.ptrValue);
        break;
        
    default:
        break;
    }

    type = newType;

    switch (type)
    {
    case VAR_STRING:
        new(reinterpret_cast<String*>(&data)) String();
        break;

    case VAR_BUFFER:
        new(reinterpret_cast<Vector<unsigned char>*>(&data)) Vector<unsigned char>();
        break;

    case VAR_RESOURCEREF:
        new(reinterpret_cast<ResourceRef*>(&data)) ResourceRef();
        break;

    case VAR_RESOURCEREFLIST:
        new(reinterpret_cast<ResourceRefList*>(&data)) ResourceRefList();
        break;

    case VAR_VARIANTVECTOR:
        new(reinterpret_cast<VariantVector*>(&data)) VariantVector();
        break;

    case VAR_VARIANTMAP:
        new(reinterpret_cast<VariantMap*>(&data)) VariantMap();
        break;

    case VAR_PTR:
        new(reinterpret_cast<WeakPtr<WeakRefCounted>*>(&data)) WeakPtr<WeakRefCounted>();
        break;
        
    case VAR_MATRIX3:
        data.ptrValue = new Matrix3();
        break;
        
    case VAR_MATRIX3X4:
        data.ptrValue = new Matrix3x4();
        break;
        
    case VAR_MATRIX4:
        data.ptrValue = new Matrix4();
        break;
        
    default:
        break;
    }
}

template<> int Variant::As<int>() const
{
    return AsInt();
}

template<> unsigned Variant::As<unsigned>() const
{
    return AsUInt();
}

template<> StringHash Variant::As<StringHash>() const
{
    return AsStringHash();
}

template<> bool Variant::As<bool>() const
{
    return AsBool();
}

template<> float Variant::As<float>() const
{
    return AsFloat();
}

template<> const Vector2& Variant::As<const Vector2&>() const
{
    return AsVector2();
}

template<> const Vector3& Variant::As<const Vector3&>() const
{
    return AsVector3();
}

template<> const Vector4& Variant::As<const Vector4&>() const
{
    return AsVector4();
}

template<> const Quaternion& Variant::As<const Quaternion&>() const
{
    return AsQuaternion();
}

template<> const Color& Variant::As<const Color&>() const
{
    return AsColor();
}

template<> const String& Variant::As<const String&>() const
{
    return AsString();
}

template<> const IntRect& Variant::As<const IntRect&>() const
{
    return AsIntRect();
}

template<> const IntVector2& Variant::As<const IntVector2&>() const
{
    return AsIntVector2();
}

template<> const Vector<unsigned char>& Variant::As<const Vector<unsigned char>& >() const
{
    return AsBuffer();
}

template<> void* Variant::As<void*>() const
{
    return AsVoidPtr();
}

template<> WeakRefCounted* Variant::As<WeakRefCounted*>() const
{
    return AsPtr();
}

template<> const Matrix3& Variant::As<const Matrix3&>() const
{
    return AsMatrix3();
}

template<> const Matrix3x4& Variant::As<const Matrix3x4&>() const
{
    return AsMatrix3x4();
}

template<> const Matrix4& Variant::As<const Matrix4&>() const
{
    return AsMatrix4();
}

template<> ResourceRef Variant::As<ResourceRef>() const
{
    return AsResourceRef();
}

template<> ResourceRefList Variant::As<ResourceRefList>() const
{
    return AsResourceRefList();
}

template<> VariantVector Variant::As<VariantVector>() const
{
    return AsVariantVector();
}

template<> VariantMap Variant::As<VariantMap>() const
{
    return AsVariantMap();
}

template<> Vector2 Variant::As<Vector2>() const
{
    return AsVector2();
}

template<> Vector3 Variant::As<Vector3>() const
{
    return AsVector3();
}

template<> Vector4 Variant::As<Vector4>() const
{
    return AsVector4();
}

template<> Quaternion Variant::As<Quaternion>() const
{
    return AsQuaternion();
}

template<> Color Variant::As<Color>() const
{
    return AsColor();
}

template<> String Variant::As<String>() const
{
    return AsString();
}

template<> IntRect Variant::As<IntRect>() const
{
    return AsIntRect();
}

template<> IntVector2 Variant::As<IntVector2>() const
{
    return AsIntVector2();
}

template<> Vector<unsigned char> Variant::As<Vector<unsigned char> >() const
{
    return AsBuffer();
}

template<> Matrix3 Variant::As<Matrix3>() const
{
    return AsMatrix3();
}

template<> Matrix3x4 Variant::As<Matrix3x4>() const
{
    return AsMatrix3x4();
}

template<> Matrix4 Variant::As<Matrix4>() const
{
    return AsMatrix4();
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
