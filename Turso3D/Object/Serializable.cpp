// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/JSONValue.h"
#include "../IO/ObjectRef.h"
#include "../IO/Stream.h"
#include "ObjectResolver.h"
#include "Serializable.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

HashMap<StringHash, Vector<SharedPtr<Attribute> > > Serializable::classAttributes;

void Serializable::Load(Stream& source, ObjectResolver& resolver)
{
    const Vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return; // Nothing to do
    
    size_t numAttrs = source.ReadVLE();
    for (size_t i = 0; i < numAttrs; ++i)
    {
        // Skip attribute if wrong type or extra data
        AttributeType type = (AttributeType)source.Read<unsigned char>();
        bool skip = true;
        
        if (i < attributes->Size())
        {
            Attribute* attr = attributes->At(i);
            if (attr->Type() == type)
            {
                // Store object refs to the resolver instead of immediately setting
                if (type != ATTR_OBJECTREF)
                    attr->FromBinary(this, source);
                else
                    resolver.StoreObjectRef(this, attr, source.Read<ObjectRef>());
                
                skip = false;
            }
        }
        
        if (skip)
            Attribute::Skip(type, source);
    }
}

void Serializable::Save(Stream& dest)
{
    const Vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return;
    
    dest.WriteVLE(attributes->Size());
    for (auto it = attributes->Begin(); it != attributes->End(); ++it)
    {
        Attribute* attr = *it;
        dest.Write<unsigned char>((unsigned char)attr->Type());
        attr->ToBinary(this, dest);
    }
}

void Serializable::LoadJSON(const JSONValue& source, ObjectResolver& resolver)
{
    const Vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes || !source.IsObject() || !source.Size())
        return;
    
    const JSONObject& object = source.GetObject();
    
    for (auto it = attributes->Begin(); it != attributes->End(); ++it)
    {
        Attribute* attr = *it;
        auto jsonIt = object.Find(attr->Name());
        if (jsonIt != object.End())
        {
            // Store object refs to the resolver instead of immediately setting
            if (attr->Type() != ATTR_OBJECTREF)
                attr->FromJSON(this, jsonIt->second);
            else
                resolver.StoreObjectRef(this, attr, ObjectRef((unsigned)jsonIt->second.GetNumber()));
        }
    }
}

void Serializable::SaveJSON(JSONValue& dest)
{
    const Vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return;
    
    for (size_t i = 0; i < attributes->Size(); ++i)
    {
        Attribute* attr = attributes->At(i);
        // For better readability, do not save default-valued attributes to JSON
        if (!attr->IsDefault(this))
            attr->ToJSON(this, dest[attr->Name()]);
    }
}

void Serializable::SetAttributeValue(Attribute* attr, const void* source)
{
    if (attr)
        attr->FromValue(this, source);
}

void Serializable::AttributeValue(Attribute* attr, void* dest)
{
    if (attr)
        attr->ToValue(this, dest);
}

const Vector<SharedPtr<Attribute> >* Serializable::Attributes() const
{
    auto it = classAttributes.Find(Type());
    return it != classAttributes.End() ? &it->second : nullptr;
}

Attribute* Serializable::FindAttribute(const String& name) const
{
    return FindAttribute(name.CString());
}

Attribute* Serializable::FindAttribute(const char* name) const
{
    const Vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return 0;
    
    for (size_t i = 0; i < attributes->Size(); ++i)
    {
        Attribute* attr = attributes->At(i);
        if (attr->Name() == name)
            return attr;
    }
    
    return 0;
}

void Serializable::RegisterAttribute(StringHash type, Attribute* attr)
{
    Vector<SharedPtr<Attribute> >& attributes = classAttributes[type];
    for (size_t i = 0; i < attributes.Size(); ++i)
    {
        if (attributes[i]->Name() == attr->Name())
        {
            attributes.Insert(i, attr);
            return;
        }
    }
    
    attributes.Push(attr);
}

void Serializable::CopyBaseAttributes(StringHash type, StringHash baseType)
{
    classAttributes[type].Push(classAttributes[baseType]);
}

void Serializable::Skip(Stream& source)
{
    size_t numAttrs = source.ReadVLE();
    for (size_t i = 0; i < numAttrs; ++i)
    {
        AttributeType type = (AttributeType)source.Read<unsigned char>();
        Attribute::Skip(type, source);
    }
}

}
