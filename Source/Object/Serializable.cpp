// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/JSONValue.h"
#include "../IO/ObjectRef.h"
#include "../IO/Stream.h"
#include "ObjectResolver.h"
#include "Serializable.h"

std::map<StringHash, std::vector<SharedPtr<Attribute> > > Serializable::classAttributes;

void Serializable::Load(Stream& source, ObjectResolver& resolver)
{
    const std::vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return; // Nothing to do
    
    size_t numAttrs = source.ReadVLE();
    for (size_t i = 0; i < numAttrs; ++i)
    {
        // Skip attribute if wrong type or extra data
        AttributeType type = (AttributeType)source.Read<unsigned char>();
        bool skip = true;
        
        if (i < attributes->size())
        {
            Attribute* attr = attributes->at(i);
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
    const std::vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return;
    
    dest.WriteVLE(attributes->size());
    for (auto it = attributes->begin(); it != attributes->end(); ++it)
    {
        Attribute* attr = *it;
        dest.Write<unsigned char>((unsigned char)attr->Type());
        attr->ToBinary(this, dest);
    }
}

void Serializable::LoadJSON(const JSONValue& source, ObjectResolver& resolver)
{
    const std::vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes || !source.IsObject() || !source.Size())
        return;
    
    const JSONObject& object = source.GetObject();
    
    for (auto it = attributes->begin(); it != attributes->end(); ++it)
    {
        Attribute* attr = *it;
        auto jsonIt = object.find(attr->Name());
        if (jsonIt != object.end())
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
    const std::vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return;
    
    for (size_t i = 0; i < attributes->size(); ++i)
    {
        Attribute* attr = attributes->at(i);
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

const std::vector<SharedPtr<Attribute> >* Serializable::Attributes() const
{
    auto it = classAttributes.find(Type());
    return it != classAttributes.end() ? &it->second : nullptr;
}

Attribute* Serializable::FindAttribute(const std::string& name) const
{
    return FindAttribute(name.c_str());
}

Attribute* Serializable::FindAttribute(const char* name) const
{
    const std::vector<SharedPtr<Attribute> >* attributes = Attributes();
    if (!attributes)
        return nullptr;
    
    for (size_t i = 0; i < attributes->size(); ++i)
    {
        Attribute* attr = attributes->at(i);
        if (attr->Name() == name)
            return attr;
    }
    
    return nullptr;
}

void Serializable::RegisterAttribute(StringHash type, Attribute* attr)
{
    std::vector<SharedPtr<Attribute> >& attributes = classAttributes[type];
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        if (attributes[i]->Name() == attr->Name())
        {
            attributes.insert(attributes.begin() + i, attr);
            return;
        }
    }
    
    attributes.push_back(attr);
}

void Serializable::CopyBaseAttributes(StringHash type, StringHash baseType)
{
    // Make sure the types are different, which may not be true if the OBJECT macro has been omitted
    if (type != baseType)
    {
        std::vector<SharedPtr<Attribute> >& attributes = classAttributes[baseType];
        for (size_t i = 0; i < attributes.size(); ++i)
            RegisterAttribute(type, attributes[i]);
    }
}

void Serializable::CopyBaseAttribute(StringHash type, StringHash baseType, const std::string& name)
{
    // Make sure the types are different, which may not be true if the OBJECT macro has been omitted
    if (type != baseType)
    {
        std::vector<SharedPtr<Attribute> >& attributes = classAttributes[baseType];
        for (size_t i = 0; i < attributes.size(); ++i)
        {
            if (attributes[i]->Name() == name)
            {
                RegisterAttribute(type, attributes[i]);
                break;
            }
        }
    }
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
