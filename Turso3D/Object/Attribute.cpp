// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/JSONValue.h"
#include "../IO/ObjectRef.h"
#include "../IO/ResourceRef.h"
#include "../Math/BoundingBox.h"
#include "../Math/Quaternion.h"
#include "Attribute.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

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
    switch (type)
    {
    case ATTR_BOOL:
        source.Read<bool>();
        break;

    case ATTR_INT:
        source.Read<int>();
        break;

    case ATTR_UNSIGNED:
        source.Read<unsigned>();
        break;
        
    case ATTR_BYTE:
        source.Read<unsigned char>();
        break;

    case ATTR_FLOAT:
        source.Read<float>();
        break;

    case ATTR_STRING:
        source.Read<String>();
        break;

    case ATTR_VECTOR3:
        source.Read<Vector3>();
        break;
        
    case ATTR_VECTOR4:
        source.Read<Vector4>();
        break;
        
    case ATTR_QUATERNION:
        source.Read<Quaternion>();
        break;
        
    case ATTR_BOUNDINGBOX:
        source.Read<BoundingBox>();
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

template<> TURSO3D_API void AttributeImpl<bool>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, source.GetBool());
}

template<> TURSO3D_API void AttributeImpl<int>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, (int)source.GetNumber());
}

template<> TURSO3D_API void AttributeImpl<unsigned>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, (unsigned)source.GetNumber());
}

template<> TURSO3D_API void AttributeImpl<unsigned char>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, (unsigned char)source.GetNumber());
}

template<> TURSO3D_API void AttributeImpl<float>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, (float)source.GetNumber());
}

template<> TURSO3D_API void AttributeImpl<String>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, source.GetString());
}

template<> TURSO3D_API void AttributeImpl<Vector3>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, Vector3(source.GetString()));
}

template<> TURSO3D_API void AttributeImpl<Vector4>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, Vector4(source.GetString()));
}

template<> TURSO3D_API void AttributeImpl<Quaternion>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, Quaternion(source.GetString()));
}

template<> TURSO3D_API void AttributeImpl<BoundingBox>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, BoundingBox(source.GetString()));
}

template<> TURSO3D_API void AttributeImpl<ResourceRef>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, ResourceRef(source.GetString()));
}

template<> TURSO3D_API void AttributeImpl<ResourceRefList>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, ResourceRefList(source.GetString()));
}

template<> TURSO3D_API void AttributeImpl<ObjectRef>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, ObjectRef((unsigned)source.GetNumber()));
}

template<> TURSO3D_API void AttributeImpl<JSONValue>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, source);
}

template<> TURSO3D_API void AttributeImpl<bool>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> TURSO3D_API void AttributeImpl<int>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> TURSO3D_API void AttributeImpl<unsigned>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> TURSO3D_API void AttributeImpl<unsigned char>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> TURSO3D_API void AttributeImpl<float>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> TURSO3D_API void AttributeImpl<String>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> TURSO3D_API void AttributeImpl<Vector3>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> TURSO3D_API void AttributeImpl<Vector4>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> TURSO3D_API void AttributeImpl<Quaternion>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> TURSO3D_API void AttributeImpl<BoundingBox>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> TURSO3D_API void AttributeImpl<ResourceRef>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> TURSO3D_API void AttributeImpl<ResourceRefList>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> TURSO3D_API void AttributeImpl<ObjectRef>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).id;
}

template<> TURSO3D_API void AttributeImpl<JSONValue>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> TURSO3D_API AttributeType AttributeImpl<bool>::Type() const
{
    return ATTR_BOOL;
}

template<> TURSO3D_API AttributeType AttributeImpl<int>::Type() const
{
    return ATTR_INT;
}

template<> TURSO3D_API AttributeType AttributeImpl<unsigned>::Type() const
{
    return ATTR_UNSIGNED;
}

template<> TURSO3D_API AttributeType AttributeImpl<unsigned char>::Type() const
{
    return ATTR_BYTE;
}

template<> TURSO3D_API AttributeType AttributeImpl<float>::Type() const
{
    return ATTR_FLOAT;
}

template<> TURSO3D_API AttributeType AttributeImpl<String>::Type() const
{
    return ATTR_STRING;
}

template<> TURSO3D_API AttributeType AttributeImpl<Vector3>::Type() const
{
    return ATTR_VECTOR3;
}

template<> TURSO3D_API AttributeType AttributeImpl<Vector4>::Type() const
{
    return ATTR_VECTOR4;
}

template<> TURSO3D_API AttributeType AttributeImpl<Quaternion>::Type() const
{
    return ATTR_QUATERNION;
}

template<> TURSO3D_API AttributeType AttributeImpl<BoundingBox>::Type() const
{
    return ATTR_BOUNDINGBOX;
}

template<> TURSO3D_API AttributeType AttributeImpl<ResourceRef>::Type() const
{
    return ATTR_RESOURCEREF;
}

template<> TURSO3D_API AttributeType AttributeImpl<ResourceRefList>::Type() const
{
    return ATTR_RESOURCEREFLIST;
}

template<> TURSO3D_API AttributeType AttributeImpl<ObjectRef>::Type() const
{
    return ATTR_OBJECTREF;
}

template<> TURSO3D_API AttributeType AttributeImpl<JSONValue>::Type() const
{
    return ATTR_JSONVALUE;
}

}
