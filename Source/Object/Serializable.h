// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Attribute.h"
#include "Object.h"

class ObjectResolver;

/// Base class for objects with automatic serialization using attributes.
class Serializable : public Object
{
public:
    /// Load from binary stream. Store object ref attributes to be resolved later.
    virtual void Load(Stream& source, ObjectResolver& resolver);
    /// Save to binary stream.
    virtual void Save(Stream& dest);
    /// Load from JSON data. Optionally store object ref attributes to be resolved later.
    virtual void LoadJSON(const JSONValue& source, ObjectResolver& resolver);
    /// Save as JSON data.
    virtual void SaveJSON(JSONValue& dest);
    /// Return id for referring to the object in serialization.
    virtual unsigned Id() const { return 0; }

    /// Set attribute value from memory.
    void SetAttributeValue(Attribute* attr, const void* source);
    /// Copy attribute value to memory.
    void AttributeValue(Attribute* attr, void* dest);
    
    /// Set attribute value, template version. Return true if value was right type.
    template <class T> bool SetAttributeValue(Attribute* attr, const T& source)
    {
        AttributeImpl<T>* typedAttr = dynamic_cast<AttributeImpl<T>*>(attr);
        if (typedAttr)
        {
            typedAttr->SetValue(this, source);
            return true;
        }
        else
            return false;
    }
    
    /// Copy attribute value, template version. Return true if value was right type.
    template <class T> bool AttributeValue(Attribute* attr, T& dest)
    {
        AttributeImpl<T>* typedAttr = dynamic_cast<AttributeImpl<T>*>(attr);
        if (typedAttr)
        {
            typedAttr->Value(this, dest);
            return true;
        }
        else
            return false;
    }
    
    /// Return attribute value, template version.
    template <class T> T AttributeValue(Attribute* attr)
    {
        AttributeImpl<T>* typedAttr = dynamic_cast<AttributeImpl<T>*>(attr);
        return typedAttr ? typedAttr->Value(this) : T();
    }
    
    /// Return the attribute descriptions. Default implementation uses per-class registration.
    virtual const std::vector<SharedPtr<Attribute> >* Attributes() const;
    /// Return an attribute description by name, or null if does not exist.
    Attribute* FindAttribute(const std::string& name) const;
    /// Return an attribute description by name, or null if does not exist.
    Attribute* FindAttribute(const char* name) const;
    
    /// Register a per-class attribute. If an attribute with the same name already exists, it will be replaced.
    static void RegisterAttribute(StringHash type, Attribute* attr);
    /// Copy all base class attributes.
    static void CopyBaseAttributes(StringHash type, StringHash baseType);
    /// Copy one base class attribute.
    static void CopyBaseAttribute(StringHash type, StringHash baseType, const std::string& name);
    /// Skip binary data of an object's all attributes.
    static void Skip(Stream& source);
    
    /// Register a per-class attribute, template version. Should not be used for base class attributes unless the type is explicitly specified, as by default the attribute will be re-registered to the base class redundantly.
    template <class T, class U> static void RegisterAttribute(const char* name, U (T::*getFunction)() const, void (T::*setFunction)(U), const U& defaultValue = U(), const char** enumNames = 0)
    {
        RegisterAttribute(T::TypeStatic(), new AttributeImpl<U>(name, new AttributeAccessorImpl<T, U>(getFunction, setFunction), defaultValue, enumNames));
    }
    
    /// Register a per-class attribute with reference access, template version. Should not be used for base class attributes unless the type is explicitly specified, as by default the attribute will be re-registered to the base class redundantly.
    template <class T, class U> static void RegisterRefAttribute(const char* name, const U& (T::*getFunction)() const, void (T::*setFunction)(const U&), const U& defaultValue = U(), const char** enumNames = 0)
    {
        RegisterAttribute(T::TypeStatic(), new AttributeImpl<U>(name, new RefAttributeAccessorImpl<T, U>(getFunction, setFunction), defaultValue, enumNames));
    }

    /// Register a per-class attribute with mixed reference access, template version. Should not be used for base class attributes unless the type is explicitly specified, as by default the attribute will be re-registered to the base class redundantly.
    template <class T, class U> static void RegisterMixedRefAttribute(const char* name, U (T::*getFunction)() const, void (T::*setFunction)(const U&), const U& defaultValue = U(), const char** enumNames = 0)
    {
        RegisterAttribute(T::TypeStatic(), new AttributeImpl<U>(name, new MixedRefAttributeAccessorImpl<T, U>(getFunction, setFunction), defaultValue, enumNames));
    }

    /// Copy all base class attributes, template version.
    template <class T, class U> static void CopyBaseAttributes()
    {
        CopyBaseAttributes(T::TypeStatic(), U::TypeStatic());
    }

    /// Copy one base class attribute, template version.
    template <class T, class U> static void CopyBaseAttribute(const std::string& name)
    {
        CopyBaseAttribute(T::TypeStatic(), U::TypeStatic(), name);
    }
    
private:
    /// Per-class attributes.
    static std::map<StringHash, std::vector<SharedPtr<Attribute> > > classAttributes;
};
