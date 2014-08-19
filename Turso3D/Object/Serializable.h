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
    /// Return an attribute description by name, or null if does not exist.
    const Attribute* FindAttribute(const String& name) const;
    /// Return an attribute description by name, or null if does not exist.
    const Attribute* FindAttribute(const char* name) const;
    
    /// Register a per-class attribute. If an attribute with the same name already exists, it will be replaced.
    static void RegisterAttribute(StringHash type, Attribute* attr);
    
    /// Register a per-class attribute, template version.
    template <class T, class U> static void RegisterAttribute(const char* name, U (T::*getFunction)() const, void (T::*setFunction)(U), const U& defaultValue = U(), const char** enumNames = 0)
    {
        RegisterAttribute(T::TypeStatic(), new AttributeImpl<U>(name, new AttributeAccessorImpl<T, U>(getFunction, setFunction), defaultValue, enumNames));
    }
    
    /// Register a per-class attribute with reference access, template version.
    template <class T, class U> static void RegisterAttribute(const char* name, const U& (T::*getFunction)() const, void (T::*setFunction)(const U&), const U& defaultValue = U(), const char** enumNames = 0)
    {
        RegisterAttribute(T::TypeStatic(), new AttributeImpl<U>(name, new RefAttributeAccessorImpl<T, U>(getFunction, setFunction), defaultValue, enumNames));
    }
    
private:
    /// Per-class attributes.
    static HashMap<StringHash, Vector<AutoPtr<Attribute> > > classAttributes;
};

}
