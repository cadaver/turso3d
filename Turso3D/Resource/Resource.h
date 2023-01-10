// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "../IO/ResourceRef.h"
#include "../Object/Object.h"

class Stream;

/// Base class for resources.
class Resource : public Object
{
    OBJECT(Resource);

public:
    /// Load the resource data from a stream. May be executed outside the main thread, should not access GPU resources. Return true on success.
    virtual bool BeginLoad(Stream& source);
    /// Finish resource loading if necessary. Always called from the main thread, so GPU resources can be accessed here. Return true on success.
    virtual bool EndLoad();
    /// Save the resource to a stream. Return true on success.
    virtual bool Save(Stream& dest);

    /// Load the resource synchronously from a binary stream. Return true on success.
    bool Load(Stream& source);
    /// Set name of the resource, usually the same as the file being loaded from.
    void SetName(const std::string& newName);

    /// Return name of the resource.
    const std::string& Name() const { return name; }
    /// Return name hash of the resource.
    const StringHash& NameHash() const { return nameHash; }

private:
    /// Resource name.
    std::string name;
    /// Resource name hash.
    StringHash nameHash;
};

/// Return name from a resource pointer.
inline const std::string& ResourceName(Resource* resource)
{
    return resource ? resource->Name() : JSONValue::emptyString;
}

/// Return type from a resource pointer, or default type if null.
inline StringHash ResourceType(Resource* resource, StringHash defaultType)
{
    return resource ? resource->Type() : defaultType;
}

/// Make a resource ref from a resource pointer.
inline ResourceRef MakeResourceRef(Resource* resource, StringHash defaultType)
{
    return ResourceRef(ResourceType(resource, defaultType), ResourceName(resource));
}

/// Return resource names from a vector of resource pointers.
template <class T> std::vector<std::string> ResourceNames(const std::vector<T*>& resources)
{
    std::vector<std::string> ret(resources.size());
    for (size_t i = 0; i < resources.size(); ++i)
        ret[i] = ResourceName(resources[i]);

    return ret;
}

/// Make a resource ref list from a vector of resource poitners.
template <class T> ResourceRefList MakeResourceRefList(const std::vector<T*>& resources)
{
    return ResourceRefList(T::TypeStatic(), GetResourceNames(resources));
}
