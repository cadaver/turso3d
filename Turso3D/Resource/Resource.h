// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/ResourceRef.h"
#include "../Object/Object.h"

namespace Turso3D
{

class Stream;

/// Base class for resources.
class TURSO3D_API Resource : public Object
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
    void SetName(const String& newName);

    /// Return name of the resource.
    const String& Name() const { return name; }
    /// Return name hash of the resource.
    const StringHash& NameHash() const { return nameHash; }

private:
    /// Resource name.
    String name;
    /// Resource name hash.
    StringHash nameHash;
};

/// Return name from a resource pointer.
inline const String& ResourceName(Resource* resource)
{
    return resource ? resource->Name() : String::EMPTY;
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
template <class T> Vector<String> ResourceNames(const Vector<T*>& resources)
{
    Vector<String> ret(resources.Size());
    for (size_t i = 0; i < resources.Size(); ++i)
        ret[i] = ResourceName(resources[i]);

    return ret;
}

/// Make a resource ref list from a vector of resource poitners.
template <class T> ResourceRefList MakeResourceRefList(const Vector<T*>& resources)
{
    return ResourceRefList(T::GetTypeStatic(), GetResourceNames(resources));
}

}
