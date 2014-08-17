// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/JSONValue.h"
#include "Attribute.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

void Attribute::FromValue(Serializable* instance, const void* source) const
{
    accessor->Set(instance, source);
}

void Attribute::ToValue(Serializable* instance, void* dest) const
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

    case ATTR_FLOAT:
        source.Read<float>();
        break;

    case ATTR_STRING:
        source.Read<String>();
        break;

    default:
        break;
    }
}

template<> void AttributeImpl<bool>::FromJSON(Serializable* instance, const JSONValue& source) const
{
    SetValue(instance, source.GetBool());
}

template<> void AttributeImpl<bool>::ToJSON(Serializable* instance, JSONValue& dest) const
{
    dest = Value(instance);
}

template<> AttributeType AttributeImpl<bool>::Type() const
{
    return ATTR_BOOL;
}

template<> void AttributeImpl<int>::FromJSON(Serializable* instance, const JSONValue& source) const
{
    SetValue(instance, (int)source.GetNumber());
}

template<> void AttributeImpl<int>::ToJSON(Serializable* instance, JSONValue& dest) const
{
    dest = Value(instance);
}

template<> AttributeType AttributeImpl<int>::Type() const
{
    return ATTR_INT;
}

template<> void AttributeImpl<float>::FromJSON(Serializable* instance, const JSONValue& source) const
{
    SetValue(instance, (float)source.GetNumber());
}

template<> void AttributeImpl<float>::ToJSON(Serializable* instance, JSONValue& dest) const
{
    dest = Value(instance);
}

template<> AttributeType AttributeImpl<float>::Type() const
{
    return ATTR_FLOAT;
}

template<> void AttributeImpl<String>::FromJSON(Serializable* instance, const JSONValue& source) const
{
    SetValue(instance, source.GetString());
}

template<> void AttributeImpl<String>::ToJSON(Serializable* instance, JSONValue& dest) const
{
    dest = Value(instance);
}

template<> AttributeType AttributeImpl<String>::Type() const
{
    return ATTR_STRING;
}


}
