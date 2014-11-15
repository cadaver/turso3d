// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "Image.h"
#include "JSONFile.h"
#include "ResourceCache.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

ResourceCache::ResourceCache()
{
    RegisterSubsystem(this);
}

ResourceCache::~ResourceCache()
{
    UnloadAllResources(true);
    RemoveSubsystem(this);
}

bool ResourceCache::AddResourceDir(const String& pathName, bool addFirst)
{
    PROFILE(AddResourceDir);

    if (!DirExists(pathName))
    {
        LOGERROR("Could not open directory " + pathName);
        return false;
    }

    String fixedPath = SanitateResourceDirName(pathName);

    // Check that the same path does not already exist
    for (size_t i = 0; i < resourceDirs.Size(); ++i)
    {
        if (!resourceDirs[i].Compare(fixedPath, false))
            return true;
    }

    if (addFirst)
        resourceDirs.Insert(0, fixedPath);
    else
        resourceDirs.Push(fixedPath);

    LOGINFO("Added resource path " + fixedPath);
    return true;
}

bool ResourceCache::AddManualResource(Resource* resource)
{
    if (!resource)
    {
        LOGERROR("Null manual resource");
        return false;
    }

    if (resource->Name().IsEmpty())
    {
        LOGERROR("Manual resource with empty name, can not add");
        return false;
    }

    resources[MakePair(resource->Type(), StringHash(resource->Name()))] = resource;
    return true;
}

void ResourceCache::RemoveResourceDir(const String& pathName)
{
    // Convert path to absolute
    String fixedPath = SanitateResourceDirName(pathName);

    for (size_t i = 0; i < resourceDirs.Size(); ++i)
    {
        if (!resourceDirs[i].Compare(fixedPath, false))
        {
            resourceDirs.Erase(i);
            LOGINFO("Removed resource path " + fixedPath);
            return;
        }
    }
}

void ResourceCache::UnloadResource(StringHash type, const String& name, bool force)
{
    auto key = MakePair(type, StringHash(name));
    auto it = resources.Find(key);
    if (it == resources.End())
        return;

    Resource* resource = it->second;
    if (!resource->WeakRefs() || force)
        resources.Erase(key);
}

void ResourceCache::UnloadResources(StringHash type, bool force)
{
    for (auto it = resources.Begin(); it != resources.End();)
    {
        auto current = it++;
        if (current->first.first == type)
        {
            Resource* resource = current->second;
            if (!resource->WeakRefs() || force)
                resources.Erase(current);
        }
    }
}

void ResourceCache::UnloadResources(StringHash type, const String& partialName, bool force)
{
    for (auto it = resources.Begin(); it != resources.End();)
    {
        auto current = it++;
        if (current->first.first == type)
        {
            Resource* resource = current->second;
            if (resource->Name().StartsWith(partialName) && (!resource->WeakRefs() || force))
                resources.Erase(current);
        }
    }
}

void ResourceCache::UnloadResources(const String& partialName, bool force)
{
    // Some resources refer to others, like materials to textures. Release twice to ensure these get released.
    // This is not necessary if forcing release
    size_t repeat = force ? 1 : 2;

    while (repeat--)
    {
        for (auto it = resources.Begin(); it != resources.End();)
        {
            auto current = it++;
            Resource* resource = current->second;
            if (resource->Name().StartsWith(partialName) && (!resource->WeakRefs() || force))
                resources.Erase(current);
        }
    }
}

void ResourceCache::UnloadAllResources(bool force)
{
    if (!force)
    {
        size_t repeat = 2;

        while (repeat--)
        {
            for (auto it = resources.Begin(); it != resources.End();)
            {
                auto current = it++;
                Resource* resource = current->second;
                if (!resource->WeakRefs())
                    resources.Erase(current);
            }
        }
    }
    else
        resources.Clear();
}

bool ResourceCache::ReloadResource(Resource* resource)
{
    if (!resource)
        return false;

    bool success = false;
    File file;
    if (OpenFile(file, resource->Name()))
        success = resource->Load(file);

    return success;
}

bool ResourceCache::OpenFile(File& dest, const String& nameIn)
{
    String name = SanitateResourceName(nameIn);

    for (size_t i = 0; i < resourceDirs.Size(); ++i)
    {
        if (FileExists(resourceDirs[i] + name))
        {
            // Construct the file first with full path, then rename it to not contain the resource path,
            // so that the file's name can be used in further OpenFile() calls (for example over the network)
            dest.Open(resourceDirs[i] + name);
            return dest.IsOpen();
        }
    }

    // Fallback using absolute path
    dest.Open(nameIn);
    return dest.IsOpen();
}

Resource* ResourceCache::LoadResource(StringHash type, const String& nameIn)
{
    String name = SanitateResourceName(nameIn);

    // If empty name, return null pointer immediately
    if (name.IsEmpty())
        return 0;

    // Check for existing resource
    auto key = MakePair(type, StringHash(name));
    auto it = resources.Find(key);
    if (it != resources.End())
        return it->second;

    AutoPtr<Object> newObject = Create(type);
    if (!newObject)
    {
        LOGERROR("Could not load unknown resource type " + String(type));
        return 0;
    }
    Resource* newResource = dynamic_cast<Resource*>(newObject.Get());
    if (!newResource)
    {
        LOGERROR("Type " + String(type) + " is not a resource");
        return 0;
    }

    // Attempt to load the resource
    File file;
    if (!OpenFile(file, name))
        return false;
    
    LOGDEBUG("Loading resource " + name);
    newResource->SetName(name);
    if (!newResource->Load(file))
        return 0;

    // Store to cache
    newObject.Detach();
    resources[key] = newResource;
    return newResource;
}

void ResourceCache::ResourcesByType(Vector<Resource*>& result, StringHash type) const
{
    result.Clear();

    for (auto it = resources.Begin(); it != resources.End(); ++it)
    {
        if (it->second->Type() == type)
            result.Push(it->second);
    }
}

bool ResourceCache::Exists(const String& nameIn) const
{
    String name = SanitateResourceName(nameIn);

    for (size_t i = 0; i < resourceDirs.Size(); ++i)
    {
        if (FileExists(resourceDirs[i] + name))
            return true;
    }

    // Fallback using absolute path
    return FileExists(name);
}

String ResourceCache::ResourceFileName(const String& name) const
{
    for (unsigned i = 0; i < resourceDirs.Size(); ++i)
    {
        if (FileExists(resourceDirs[i] + name))
            return resourceDirs[i] + name;
    }

    return String();
}

String ResourceCache::SanitateResourceName(const String& nameIn) const
{
    // Sanitate unsupported constructs from the resource name
    String name = NormalizePath(nameIn);
    name.Replace("../", "");
    name.Replace("./", "");

    // If the path refers to one of the resource directories, normalize the resource name
    if (resourceDirs.Size())
    {
        String namePath = Path(name);
        String exePath = ExecutableDir();
        for (unsigned i = 0; i < resourceDirs.Size(); ++i)
        {
            String relativeResourcePath = resourceDirs[i];
            if (relativeResourcePath.StartsWith(exePath))
                relativeResourcePath = relativeResourcePath.Substring(exePath.Length());

            if (namePath.StartsWith(resourceDirs[i], false))
                namePath = namePath.Substring(resourceDirs[i].Length());
            else if (namePath.StartsWith(relativeResourcePath, false))
                namePath = namePath.Substring(relativeResourcePath.Length());
        }

        name = namePath + FileNameAndExtension(name);
    }

    return name.Trimmed();
}

String ResourceCache::SanitateResourceDirName(const String& nameIn) const
{
    // Convert path to absolute
    String fixedPath = AddTrailingSlash(nameIn);
    if (!IsAbsolutePath(fixedPath))
        fixedPath = CurrentDir() + fixedPath;

    // Sanitate away /./ construct
    fixedPath.Replace("/./", "/");

    return fixedPath.Trimmed();
}

void RegisterResourceLibrary()
{
    Image::RegisterObject();
    JSONFile::RegisterObject();
}

}
