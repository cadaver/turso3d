// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Deserializer.h"
#include "../IO/JsonValue.h"
#include "../IO/Serializer.h"
#include "Serializable.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

HashMap<StringHash, Vector<AutoPtr<Attribute> > > Serializable::classAttributes;

void Serializable::Load(Deserializer& source)
{
    const Vector<AutoPtr<Attribute> >* attributes = Attributes();
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
                attr->FromBinary(this, source);
                skip = false;
            }
        }
        
        if (skip)
            Attribute::Skip(type, source);
    }
}

void Serializable::Save(Serializer& dest)
{
    const Vector<AutoPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return;
    
    dest.WriteVLE(attributes->Size());
    for (size_t i = 0; i < attributes->Size(); ++i)
    {
        Attribute* attr = attributes->At(i);
        dest.Write<unsigned char>((unsigned char)attr->Type());
        attr->ToBinary(this, dest);
    }
}

void Serializable::LoadJSON(const JSONValue& source)
{
    const Vector<AutoPtr<Attribute> >* attributes = Attributes();
    if (!attributes || !source.IsObject() || !source.Size())
        return;
    
    const JSONObject& object = source.GetObject();
    
    for (size_t i = 0; i < attributes->Size(); ++i)
    {
        Attribute* attr = attributes->At(i);
        JSONObject::ConstIterator j = object.Find(attr->Name());
        if (j != object.End())
            attr->FromJSON(this, j->second);
    }
}

void Serializable::SaveJSON(JSONValue& dest)
{
    const Vector<AutoPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return;
    
    // Clear any existing data
    dest = JSONObject();
    
    for (size_t i = 0; i < attributes->Size(); ++i)
    {
        Attribute* attr = attributes->At(i);
        if (!attr->IsDefault(this))
            attr->ToJSON(this, dest[attr->Name()]);
    }
}

void Serializable::SetAttributeValue(const Attribute* attr, const void* source)
{
    if (attr)
        attr->FromValue(this, source);
}

void Serializable::AttributeValue(const Attribute* attr, void* dest)
{
    if (attr)
        attr->ToValue(this, dest);
}

const Vector<AutoPtr<Attribute> >* Serializable::Attributes() const
{
    HashMap<StringHash, Vector<AutoPtr<Attribute> > >::ConstIterator i = classAttributes.Find(Type());
    return i != classAttributes.End() ? &i->second : (Vector<AutoPtr<Attribute> >*)0;
}

const Attribute* Serializable::FindAttribute(const String& name) const
{
    return FindAttribute(name.CString());
}

const Attribute* Serializable::FindAttribute(const char* name) const
{
    const Vector<AutoPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return 0;
    
    for (size_t i = 0; i < attributes->Size(); ++i)
    {
        Attribute* attr = attributes->At(i);
        if (!String::Compare(attr->Name().CString(), name))
            return attr;
    }
    
    return 0;
}

void Serializable::RegisterAttribute(StringHash type, Attribute* attr)
{
    Vector<AutoPtr<Attribute> >& attributes = classAttributes[type];
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

void Serializable::Skip(Deserializer& source)
{
    size_t numAttrs = source.ReadVLE();
    for (size_t i = 0; i < numAttrs; ++i)
    {
        AttributeType type = (AttributeType)source.Read<unsigned char>();
        Attribute::Skip(type, source);
    }
}

}
