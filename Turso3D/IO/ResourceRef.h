// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/StringHash.h"
#include "../Base/Vector.h"

namespace Turso3D
{

/// Typed resource reference.
struct TURSO3D_API ResourceRef
{
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
    ResourceRef(const String& str)
    {
        FromString(str);
    }
    
    /// Construct from a C string.
    ResourceRef(const char* str)
    {
        FromString(str);
    }
    
    /// Construct with type and resource name.
    ResourceRef(StringHash type, const String& name_) :
        type(type),
        name(name_)
    {
    }

    /// Set from a string that contains the type and name separated by a semicolon. Return true on success.
    bool FromString(const String& str);
    /// Set from a C string that contains the type and name separated by a semicolon. Return true on success.
    bool FromString(const char* str);
    
    /// Return as a string.
    String ToString() const;
    
    /// Object type.
    StringHash type;
    /// Object name.
    String name;

    /// Test for equality with another reference.
    bool operator == (const ResourceRef& rhs) const { return type == rhs.type && name == rhs.name; }
    /// Test for inequality with another reference.
    bool operator != (const ResourceRef& rhs) const { return !(*this == rhs); }
};

/// %List of typed resource references.
struct TURSO3D_API ResourceRefList
{
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
    ResourceRefList(const String& str)
    {
        FromString(str);
    }
    
    /// Construct from a C string.
    ResourceRefList(const char* str)
    {
        FromString(str);
    }

    /// Construct with type and name list.
    ResourceRefList(StringHash type, const Vector<String>& names_) :
        type(type),
        names(names_)
    {
    }

    /// Set from a string that contains the type and names separated by semicolons. Return true on success.
    bool FromString(const String& str);
    /// Set from a C string that contains the type and names separated by semicolons. Return true on success.
    bool FromString(const char* str);
    
    /// Return as a string.
    String ToString() const;
    
    /// Object type.
    StringHash type;
    /// List of object names.
    Vector<String> names;

    /// Test for equality with another reference list.
    bool operator == (const ResourceRefList& rhs) const { return type == rhs.type && names == rhs.names; }
    /// Test for inequality with another reference list.
    bool operator != (const ResourceRefList& rhs) const { return !(*this == rhs); }
};

}
