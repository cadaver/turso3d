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

void Attribute::Skip(AttributeType type, Deserializer& source)
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

template<> void AttributeImpl<bool>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, source.GetBool());
}

template<> void AttributeImpl<int>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, (int)source.GetNumber());
}

template<> void AttributeImpl<unsigned>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, (unsigned)source.GetNumber());
}

template<> void AttributeImpl<unsigned char>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, (unsigned char)source.GetNumber());
}

template<> void AttributeImpl<float>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, (float)source.GetNumber());
}

template<> void AttributeImpl<String>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, source.GetString());
}

template<> void AttributeImpl<Vector3>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, Vector3(source.GetString()));
}

template<> void AttributeImpl<Quaternion>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, Quaternion(source.GetString()));
}

template<> void AttributeImpl<BoundingBox>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, BoundingBox(source.GetString()));
}

template<> void AttributeImpl<ResourceRef>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, ResourceRef(source.GetString()));
}

template<> void AttributeImpl<ResourceRefList>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, ResourceRefList(source.GetString()));
}

template<> void AttributeImpl<ObjectRef>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, ObjectRef((unsigned)source.GetNumber()));
}

template<> void AttributeImpl<JSONValue>::FromJSON(Serializable* instance, const JSONValue& source)
{
    SetValue(instance, source);
}

template<> void AttributeImpl<bool>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> void AttributeImpl<int>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> void AttributeImpl<unsigned>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> void AttributeImpl<unsigned char>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> void AttributeImpl<float>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> void AttributeImpl<String>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> void AttributeImpl<Vector3>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> void AttributeImpl<Quaternion>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> void AttributeImpl<BoundingBox>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> void AttributeImpl<ResourceRef>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> void AttributeImpl<ResourceRefList>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).ToString();
}

template<> void AttributeImpl<ObjectRef>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance).id;
}

template<> void AttributeImpl<JSONValue>::ToJSON(Serializable* instance, JSONValue& dest)
{
    dest = Value(instance);
}

template<> AttributeType AttributeImpl<bool>::Type() const
{
    return ATTR_BOOL;
}

template<> AttributeType AttributeImpl<int>::Type() const
{
    return ATTR_INT;
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

template<> AttributeType AttributeImpl<String>::Type() const
{
    return ATTR_STRING;
}

template<> AttributeType AttributeImpl<Vector3>::Type() const
{
    return ATTR_VECTOR3;
}

template<> AttributeType AttributeImpl<Quaternion>::Type() const
{
    return ATTR_QUATERNION;
}

template<> AttributeType AttributeImpl<BoundingBox>::Type() const
{
    return ATTR_BOUNDINGBOX;
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

}
