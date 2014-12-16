// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/Object.h"

namespace Turso3D
{

class Resource;
class Stream;

typedef HashMap<Pair<StringHash, StringHash>, Ptr<Resource> > ResourceMap;
 
/// %Resource cache subsystem. Loads resources on demand and stores them for later access.
class TURSO3D_API ResourceCache : public Object
{
    OBJECT(ResourceCache);

public:
    /// Construct and register subsystem.
    ResourceCache();
    /// Destruct. Destroy all owned resources and unregister subsystem.
    ~ResourceCache();

    /// Add a resource directory. Return true on success.
    bool AddResourceDir(const String& pathName, bool addFirst = false);
    /// Add a manually created resource. If returns success, the resource cache takes ownership of it.
    bool AddManualResource(Resource* resource);
    /// Remove a resource directory.
    void RemoveResourceDir(const String& pathName);
    /// Open a resource file stream from the resource directories. Return a pointer to the stream, or null if not found.
    AutoPtr<Stream> OpenResource(const String& name);
    /// Load and return a resource.
    Resource* LoadResource(StringHash type, const String& name);
    /// Unload resource. Optionally force removal even if referenced.
    void UnloadResource(StringHash type, const String& name, bool force = false);
    /// Unload all resources of type.
    void UnloadResources(StringHash type, bool force = false);
    /// Unload resources by type and partial name.
    void UnloadResources(StringHash type, const String& partialName, bool force = false);
    /// Unload resources by partial name.
    void UnloadResources(const String& partialName, bool force = false);
    /// Unload all resources.
    void UnloadAllResources(bool force = false);
    /// Reload an existing resource. Return true on success.
    bool ReloadResource(Resource* resource);
    /// Load and return a resource, template version.
    template <class T> T* LoadResource(const String& name) { return static_cast<T*>(LoadResource(T::TypeStatic(), name)); }
    /// Load and return a resource, template version.
    template <class T> T* LoadResource(const char* name) { return static_cast<T*>(LoadResource(T::TypeStatic(), name)); }

    /// Return resources by type.
    void ResourcesByType(Vector<Resource*>& result, StringHash type) const;
    /// Return resource directories.
    const Vector<String>& ResourceDirs() const { return resourceDirs; }
    /// Return whether a file exists in the resource directories.
    bool Exists(const String& name) const;
    /// Return an absolute filename from a resource name.
    String ResourceFileName(const String& name) const;

    /// Return resources by type, template version.
    template <class T> void ResourcesByType(Vector<T*>& dest) const
    {
        Vector<Resource*>& resources = reinterpret_cast<Vector<Resource*>&>(dest);
        StringHash type = T::TypeStatic();
        ResourcesByType(resources, type);

        // Perform conversion of the returned pointers
        for (size_t i = 0; i < resources.Size(); ++i)
        {
            Resource* resource = resources[i];
            dest[i] = static_cast<T*>(resource);
        }
    }

    /// Normalize and remove unsupported constructs from a resource name.
    String SanitateResourceName(const String& name) const;
    /// Normalize and remove unsupported constructs from a resource directory name.
    String SanitateResourceDirName(const String& name) const;

private:
    ResourceMap resources;
    Vector<String> resourceDirs;
};

/// Register Resource related object factories and attributes.
TURSO3D_API void RegisterResourceLibrary();

}
