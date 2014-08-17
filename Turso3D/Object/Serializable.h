// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Attribute.h"
#include "Object.h"

namespace Turso3D
{

/// Base class for objects with automatic serialization using attributes.
class TURSO3D_API Serializable : public Object
{
public:
    /// Load from a binary stream.
    virtual void Load(Deserializer& source);
    /// Save to a binary stream.
    virtual void Save(Serializer& dest);
    /// Load from JSON data.
    virtual void LoadJSON(const JSONValue& source);
    /// Save to JSON data.
    virtual void SaveJSON(JSONValue& dest);
    
    /// Set attribute value from memory.
    void SetAttributeValue(const Attribute* attr, const void* source);
    /// Copy attribute value to memory.
    void AttributeValue(const Attribute* attr, void* dest);
    
    /// Set attribute value, template version.
    template <class T> void SetAttributeValue(const Attribute* attr, const T& source)
    {
        const AttributeImpl<T>* typedAttr = dynamic_cast<const AttributeImpl<T>*>(attr);
        if (typedAttr)
            typedAttr->SetValue(this, source);
    }
    
    /// Copy attribute value, template version.
    template <class T> void AttributeValue(const Attribute* attr, T& dest)
    {
        const AttributeImpl<T>* typedAttr = dynamic_cast<const AttributeImpl<T>*>(attr);
        if (typedAttr)
            typedAttr->Value(this, dest);
    }
    
    /// Return attribute value, template version.
    template <class T> T AttributeValue(const Attribute* attr)
    {
        const AttributeImpl<T>* typedAttr = dynamic_cast<const AttributeImpl<T>*>(attr);
        return typedAttr ? typedAttr->Value(this) : T();
    }
    
    /// Return the attribute descriptions. Default implementation uses per-class registration.
    virtual const Vector<AutoPtr<Attribute> >* Attributes() const;
    /// Return an attribute description by name, or null if does not exist. Names are compared case-insensitive.
    const Attribute* FindAttribute(const String& name) const;
    /// Return an attribute description by name, or null if does not exist. Names are compared case-insensitive.
    const Attribute* FindAttribute(const char* name) const;
    
    /// Register a per-class attribute.
    static void RegisterAttribute(StringHash type, Attribute* attr);
    
private:
    /// Per-class attributes.
    static HashMap<StringHash, Vector<AutoPtr<Attribute> > > classAttributes;
};

#define ATTRIBUTE(className, type, name, description, getFunction, setFunction, defaultValue) \
    Turso3D::Serializable::RegisterAttribute(new AttributeImpl<type>(name, description, new AttributeAccessorImpl<className, type>(getFunction, setFunction), defaultValue));
#define REF_ATTRIBUTE(className, type, name, description, getFunction, setFunction, defaultValue) \
    Turso3D::Serializable::RegisterAttribute(new AttributeImpl<type>(name, description, new RefAttributeAccessorImpl<className, type>(getFunction, setFunction), defaultValue));
#define ENUM_ATTRIBUTE(className, type, name, description, getFunction, setFunction, defaultValue, enumNames) \
    Turso3D::Serializable::RegisterAttribute(new AttributeImpl<type>(name, description, new AttributeAccessorImpl<className, type>(getFunction, setFunction), defaultValue, enumNames));
#define REF_ENUM_ATTRIBUTE(className, type, name, description, getFunction, setFunction, defaultValue, enumNmes) \
    Turso3D::Serializable::RegisterAttribute(new AttributeImpl<type>(name, description, new RefAttributeAccessorImpl<className, type>(getFunction, setFunction), defaultValue, enumNames));

}
