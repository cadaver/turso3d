// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/JSONValue.h"
#include "../IO/ObjectRef.h"
#include "../IO/ResourceRef.h"
#include "../IO/StringUtils.h"
#include "../Math/BoundingBox.h"
#include "../Math/Color.h"
#include "../Math/IntRect.h"
#include "../Math/IntBox.h"
#include "../Math/Matrix3x4.h"
#include "Attribute.h"

const std::string Attribute::typeNames[] =
{
    "bool",
    "byte",
    "unsigned",
    "int",
    "IntVector2",
    "IntVector3",
    "IntRect",
    "IntBox",
    "float",
    "Vector2",
    "Vector3",
    "Vector4",
    "Quaternion",
    "Color",
    "Rect",
    "BoundingBox",
    "Matrix3",
    "Matrix3x4",
    "Matrix4",
    "String",
    "ResourceRef",
    "ResourceRefList",
    "ObjectRef",
    "JSONValue",
    ""
};

const size_t Attribute::byteSizes[] =
{
    sizeof(bool),
    sizeof(unsigned char),
    sizeof(unsigned),
    sizeof(int),
    sizeof(IntVector2),
    sizeof(IntVector3),
    sizeof(IntRect),
    sizeof(IntBox),
    sizeof(float),
    sizeof(Vector2),
    sizeof(Vector3),
    sizeof(Vector4),
    sizeof(Quaternion),
    sizeof(Color),
    sizeof(Rect),
    sizeof(BoundingBox),
    sizeof(Matrix3),
    sizeof(Matrix3x4),
    sizeof(Matrix4),
    0,
    0,
    0,
    sizeof(unsigned),
    0,
    0
};

AttributeAccessor::~AttributeAccessor()
{
}

Attribute::Attribute(const char* name_, AttributeAccessor* accessor_, const char** enumNames_) :
    name(name_),
    accessor(accessor_),
    enumNames(enumNames_)
{
}

void Attribute::FromValue(Serializable* instance, const void* source)
{
    accessor->Set(instance, source);
}

void Attribute::ToValue(Serializable* instance, void* dest)
{
    accessor->Get(instance, dest);
}

void Attribute::Skip(AttributeType type, Stream& source)
{
    if (byteSizes[type])
    {
        source.Seek(source.Position() + byteSizes[type]);
        return;
    }

    switch (type)
    {
    case ATTR_STRING:
        source.Read<std::string>();
        break;

    case ATTR_RESOURCEREF:
        source.Read<ResourceRef>();
        break;

    case ATTR_RESOURCEREFLIST:
        source.Read<ResourceRefList>();
        break;

    case ATTR_OBJECTREF:
        source.Read<ObjectRef>();
        break;

    case ATTR_JSONVALUE:
        source.Read<JSONValue>();
        break;

    default:
        break;
    }
}

const std::string& Attribute::TypeName() const
{
    return typeNames[Type()];
}

size_t Attribute::ByteSize() const
{
    return byteSizes[Type()];
}

void Attribute::FromJSON(AttributeType type, void* dest, const JSONValue& source)
{
    switch (type)
    {
    case ATTR_BOOL:
        *(reinterpret_cast<bool*>(dest)) = source.GetBool();
        break;

    case ATTR_BYTE:
        *(reinterpret_cast<unsigned char*>(dest)) = (unsigned char)source.GetNumber();
        break;

    case ATTR_UNSIGNED:
        *(reinterpret_cast<unsigned*>(dest)) = (unsigned)source.GetNumber();
        break;

    case ATTR_INT:
        *(reinterpret_cast<int*>(dest)) = (int)source.GetNumber();
        break;

    case ATTR_INTVECTOR2:
        reinterpret_cast<IntVector2*>(dest)->FromString(source.GetString());
        break;

    case ATTR_INTVECTOR3:
        reinterpret_cast<IntVector3*>(dest)->FromString(source.GetString());
        break;

    case ATTR_INTRECT:
        reinterpret_cast<IntRect*>(dest)->FromString(source.GetString());
        break;

    case ATTR_INTBOX:
        reinterpret_cast<IntBox*>(dest)->FromString(source.GetString());
        break;

    case ATTR_FLOAT:
        *(reinterpret_cast<float*>(dest)) = (float)source.GetNumber();
        break;

    case ATTR_VECTOR2:
        reinterpret_cast<Vector2*>(dest)->FromString(source.GetString());
        break;

    case ATTR_VECTOR3:
        reinterpret_cast<Vector3*>(dest)->FromString(source.GetString());
        break;

    case ATTR_VECTOR4:
        reinterpret_cast<Vector4*>(dest)->FromString(source.GetString());
        break;

    case ATTR_QUATERNION:
        reinterpret_cast<Quaternion*>(dest)->FromString(source.GetString());
        break;

    case ATTR_COLOR:
        reinterpret_cast<Color*>(dest)->FromString(source.GetString());
        break;

    case ATTR_RECT:
        reinterpret_cast<Rect*>(dest)->FromString(source.GetString());
        break;

    case ATTR_BOUNDINGBOX:
        reinterpret_cast<BoundingBox*>(dest)->FromString(source.GetString());
        break;

    case ATTR_MATRIX3:
        reinterpret_cast<Matrix3*>(dest)->FromString(source.GetString());
        break;

    case ATTR_MATRIX3X4:
        reinterpret_cast<Matrix3x4*>(dest)->FromString(source.GetString());
        break;

    case ATTR_MATRIX4:
        reinterpret_cast<Matrix4*>(dest)->FromString(source.GetString());
        break;

    case ATTR_STRING:
        *(reinterpret_cast<std::string*>(dest)) = source.GetString();
        break;

    case ATTR_RESOURCEREF:
        reinterpret_cast<ResourceRef*>(dest)->FromString(source.GetString());
        break;

    case ATTR_RESOURCEREFLIST:
        reinterpret_cast<ResourceRefList*>(dest)->FromString(source.GetString());
        break;

    case ATTR_OBJECTREF:
        reinterpret_cast<ObjectRef*>(dest)->id = (unsigned)source.GetNumber();
        break;

    case ATTR_JSONVALUE:
        *(reinterpret_cast<JSONValue*>(dest)) = source;
        break;

    default:
        break;
    }
}

void Attribute::ToJSON(AttributeType type, JSONValue& dest, const void* source)
{
    switch (type)
    {
    case ATTR_BOOL:
        dest = *(reinterpret_cast<const bool*>(source));
        break;

    case ATTR_BYTE:
        dest = *(reinterpret_cast<const unsigned char*>(source));
        break;

    case ATTR_UNSIGNED:
        dest = *(reinterpret_cast<const unsigned*>(source));
        break;

    case ATTR_INT:
        dest = *(reinterpret_cast<const int*>(source));
        break;

    case ATTR_INTVECTOR2:
        dest = reinterpret_cast<const IntVector2*>(source)->ToString();
        break;

    case ATTR_INTVECTOR3:
        dest = reinterpret_cast<const IntVector3*>(source)->ToString();
        break;

    case ATTR_INTRECT:
        dest = reinterpret_cast<const IntRect*>(source)->ToString();
        break;

    case ATTR_INTBOX:
        dest = reinterpret_cast<const IntBox*>(source)->ToString();
        break;

    case ATTR_FLOAT:
        dest = *(reinterpret_cast<const float*>(source));
        break;

    case ATTR_VECTOR2:
        dest = reinterpret_cast<const Vector2*>(source)->ToString();
        break;

    case ATTR_VECTOR3:
        dest = reinterpret_cast<const Vector3*>(source)->ToString();
        break;

    case ATTR_VECTOR4:
        dest = reinterpret_cast<const Vector4*>(source)->ToString();
        break;

    case ATTR_QUATERNION:
        dest = reinterpret_cast<const Quaternion*>(source)->ToString();
        break;

    case ATTR_COLOR:
        dest = reinterpret_cast<const Color*>(source)->ToString();
        break;

    case ATTR_RECT:
        dest = reinterpret_cast<const Rect*>(source)->ToString();
        break;

    case ATTR_BOUNDINGBOX:
        dest = reinterpret_cast<const BoundingBox*>(source)->ToString();
        break;

    case ATTR_MATRIX3:
        dest = reinterpret_cast<const Matrix3*>(source)->ToString();
        break;

    case ATTR_MATRIX3X4:
        dest = reinterpret_cast<const Matrix3x4*>(source)->ToString();
        break;

    case ATTR_MATRIX4:
        dest = reinterpret_cast<const Matrix4*>(source)->ToString();
        break;

    case ATTR_STRING:
        dest = *(reinterpret_cast<const std::string*>(source));
        break;

    case ATTR_RESOURCEREF:
        dest = reinterpret_cast<const ResourceRef*>(source)->ToString();
        break;

    case ATTR_RESOURCEREFLIST:
        dest = reinterpret_cast<const ResourceRefList*>(source)->ToString();
        break;

    case ATTR_OBJECTREF:
        dest = reinterpret_cast<const ObjectRef*>(source)->id;
        break;

    case ATTR_JSONVALUE:
        dest = *(reinterpret_cast<const JSONValue*>(source));
        break;

    default:
        break;
    }
}

AttributeType Attribute::TypeFromName(const std::string& name)
{
    return (AttributeType)ListIndex(name, typeNames, MAX_ATTR_TYPES);
}

AttributeType Attribute::TypeFromName(const char* name)
{
    return (AttributeType)ListIndex(name, typeNames, MAX_ATTR_TYPES);
}

template<> AttributeType AttributeImpl<bool>::Type() const
{
    return ATTR_BOOL;
}

template<> AttributeType AttributeImpl<int>::Type() const
{
    return ATTR_INT;
}

template<> AttributeType AttributeImpl<IntVector2>::Type() const
{
    return ATTR_INTVECTOR2;
}

template<> AttributeType AttributeImpl<IntVector3>::Type() const
{
    return ATTR_INTVECTOR3;
}

template<> AttributeType AttributeImpl<IntRect>::Type() const
{
    return ATTR_INTRECT;
}

template<> AttributeType AttributeImpl<IntBox>::Type() const
{
    return ATTR_INTBOX;
}

template<> AttributeType AttributeImpl<unsigned>::Type() const
{
    return ATTR_UNSIGNED;
}

template<> AttributeType AttributeImpl<unsigned char>::Type() const
{
    return ATTR_BYTE;
}

template<> AttributeType AttributeImpl<float>::Type() const
{
    return ATTR_FLOAT;
}

template<> AttributeType AttributeImpl<std::string>::Type() const
{
    return ATTR_STRING;
}

template<> AttributeType AttributeImpl<Vector2>::Type() const
{
    return ATTR_VECTOR2;
}

template<> AttributeType AttributeImpl<Vector3>::Type() const
{
    return ATTR_VECTOR3;
}

template<> AttributeType AttributeImpl<Vector4>::Type() const
{
    return ATTR_VECTOR4;
}

template<> AttributeType AttributeImpl<Quaternion>::Type() const
{
    return ATTR_QUATERNION;
}

template<> AttributeType AttributeImpl<Color>::Type() const
{
    return ATTR_COLOR;
}

template<> AttributeType AttributeImpl<BoundingBox>::Type() const
{
    return ATTR_BOUNDINGBOX;
}

template<> AttributeType AttributeImpl<Matrix3>::Type() const
{
    return ATTR_MATRIX3;
}

template<> AttributeType AttributeImpl<Matrix3x4>::Type() const
{
    return ATTR_MATRIX3X4;
}

template<> AttributeType AttributeImpl<Matrix4>::Type() const
{
    return ATTR_MATRIX4;
}

template<> AttributeType AttributeImpl<ResourceRef>::Type() const
{
    return ATTR_RESOURCEREF;
}

template<> AttributeType AttributeImpl<ResourceRefList>::Type() const
{
    return ATTR_RESOURCEREFLIST;
}

template<> AttributeType AttributeImpl<ObjectRef>::Type() const
{
    return ATTR_OBJECTREF;
}

template<> AttributeType AttributeImpl<JSONValue>::Type() const
{
    return ATTR_JSONVALUE;
}
