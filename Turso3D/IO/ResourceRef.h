// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "StringHash.h"

#include <vector>

class Stream;

/// Typed resource reference for serialization.
struct ResourceRef
{
    /// Resource type.
    StringHash type;
    /// Resource name.
    std::string name;

    /// Construct.
    ResourceRef()
    {
    }

    // Copy-construct.
    ResourceRef(const ResourceRef& ref) :
        type(ref.type),
        name(ref.name)
    {
    }

    /// Construct from a string.
    ResourceRef(const std::string& str)
    {
        FromString(str);
    }
    
    /// Construct from a C string.
    ResourceRef(const char* str)
    {
        FromString(str);
    }
    
    /// Construct with type and resource name.
    ResourceRef(StringHash type_, const std::string& name_ = std::string()) :
        type(type_),
        name(name_)
    {
    }

    /// Set from a string that contains the type and name separated by a semicolon. Return true on success.
    bool FromString(const std::string& str);
    /// Set from a C string that contains the type and name separated by a semicolon. Return true on success.
    bool FromString(const char* string);
    /// Deserialize from a binary stream.
    void FromBinary(Stream& source);
    
    /// Return as a string.
    std::string ToString() const;
    /// Serialize to a binary stream.
    void ToBinary(Stream& dest) const;

    /// Test for equality with another reference.
    bool operator == (const ResourceRef& rhs) const { return type == rhs.type && name == rhs.name; }
    /// Test for inequality with another reference.
    bool operator != (const ResourceRef& rhs) const { return !(*this == rhs); }
};

/// %List of typed resource references for serialization.
struct ResourceRefList
{
    /// Resource type.
    StringHash type;
    /// List of resource names.
    std::vector<std::string> names;

    /// Construct.
    ResourceRefList()
    {
    }

    // Copy-construct.
    ResourceRefList(const ResourceRefList& refList) :
        type(refList.type),
        names(refList.names)
    {
    }

    /// Construct from a string.
    ResourceRefList(const std::string& str)
    {
        FromString(str);
    }
    
    /// Construct from a C string.
    ResourceRefList(const char* str)
    {
        FromString(str);
    }

    /// Construct with type and name list.
    ResourceRefList(StringHash type_, const std::vector<std::string>& names_ = std::vector<std::string>()) :
        type(type_),
        names(names_)
    {
    }

    /// Set from a string that contains the type and names separated by semicolons. Return true on success.
    bool FromString(const std::string& str);
    /// Set from a C string that contains the type and names separated by semicolons. Return true on success.
    bool FromString(const char* string);
    /// Deserialize from a binary stream.
    void FromBinary(Stream& source);

    /// Return as a string.
    std::string ToString() const;
    /// Deserialize from a binary stream.
    void ToBinary(Stream& dest) const;

    /// Test for equality with another reference list.
    bool operator == (const ResourceRefList& rhs) const { return type == rhs.type && names == rhs.names; }
    /// Test for inequality with another reference list.
    bool operator != (const ResourceRefList& rhs) const { return !(*this == rhs); }
};
