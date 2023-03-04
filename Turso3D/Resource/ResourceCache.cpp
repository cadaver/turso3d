// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../IO/StringUtils.h"
#include "Image.h"
#include "JSONFile.h"
#include "ResourceCache.h"

#include <tracy/Tracy.hpp>

ResourceCache::ResourceCache()
{
    RegisterSubsystem(this);
    RegisterResourceLibrary();
}

ResourceCache::~ResourceCache()
{
    UnloadAllResources(true);
    RemoveSubsystem(this);
}

bool ResourceCache::AddResourceDir(const std::string& pathName, bool addFirst)
{
    ZoneScoped;

    if (!DirExists(pathName))
    {
        LOGERROR("Could not open directory " + pathName);
        return false;
    }

    std::string fixedPath = SanitateResourceDirName(pathName);

    // Check that the same path does not already exist
    for (size_t i = 0; i < resourceDirs.size(); ++i)
    {
        if (resourceDirs[i] == fixedPath)
            return true;
    }

    if (addFirst)
        resourceDirs.insert(resourceDirs.begin(), fixedPath);
    else
        resourceDirs.push_back(fixedPath);

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

    if (resource->Name().empty())
    {
        LOGERROR("Manual resource with empty name, can not add");
        return false;
    }

    resources[std::make_pair(resource->Type(), StringHash(resource->Name()))] = resource;
    return true;
}

void ResourceCache::RemoveResourceDir(const std::string& pathName)
{
    // Convert path to absolute
    std::string fixedPath = SanitateResourceDirName(pathName);

    for (size_t i = 0; i < resourceDirs.size(); ++i)
    {
        if (resourceDirs[i] == fixedPath)
        {
            resourceDirs.erase(resourceDirs.begin() + i);
            LOGINFO("Removed resource path " + fixedPath);
            return;
        }
    }
}

void ResourceCache::UnloadResource(StringHash type, const std::string& name, bool force)
{
    ZoneScoped;

    auto key = std::make_pair(type, StringHash(name));
    auto it = resources.find(key);
    if (it == resources.end())
        return;

    Resource* resource = it->second;
    if (resource->Refs() == 1 || force)
        resources.erase(key);
}

void ResourceCache::UnloadResources(StringHash type, bool force)
{
    ZoneScoped;

    // In case resources refer to other resources, repeat until there are no further unloads
    for (;;)
    {
        size_t unloaded = 0;

        for (auto it = resources.begin(); it != resources.end();)
        {
            auto current = it++;
            if (current->first.first == type)
            {
                Resource* resource = current->second;
                if (resource->Refs() == 1 || force)
                {
                    resources.erase(current);
                    ++unloaded;
                }
            }
        }

        if (!unloaded)
            break;
    }
}

void ResourceCache::UnloadResources(StringHash type, const std::string& partialName, bool force)
{
    ZoneScoped;

    // In case resources refer to other resources, repeat until there are no further unloads
    for (;;)
    {
        size_t unloaded = 0;

        for (auto it = resources.begin(); it != resources.end();)
        {
            auto current = it++;
            if (current->first.first == type)
            {
                Resource* resource = current->second;
                if (StartsWith(resource->Name(), partialName) && (resource->Refs() == 1 || force))
                {
                    resources.erase(current);
                    ++unloaded;
                }
            }
        }

        if (!unloaded)
            break;
    }
}

void ResourceCache::UnloadResources(const std::string& partialName, bool force)
{
    ZoneScoped;

    // In case resources refer to other resources, repeat until there are no further unloads
    for (;;)
    {
        size_t unloaded = 0;

        for (auto it = resources.begin(); it != resources.end();)
        {
            auto current = it++;
            Resource* resource = current->second;
            if (StartsWith(resource->Name(), partialName) && (resource->Refs() == 1 || force))
            {
                resources.erase(current);
                ++unloaded;
            }
        }

        if (!unloaded)
            break;
    }
}

void ResourceCache::UnloadAllResources(bool force)
{
    ZoneScoped;

    // In case resources refer to other resources, repeat until there are no further unloads
    for (;;)
    {
        size_t unloaded = 0;

        for (auto it = resources.begin(); it != resources.end();)
        {
            auto current = it++;
            Resource* resource = current->second;
            if (resource->Refs() == 1 || force)
            {
                resources.erase(current);
                ++unloaded;
            }
        }

        if (!unloaded)
            break;
    }
}

bool ResourceCache::ReloadResource(Resource* resource)
{
    ZoneScoped;

    if (!resource)
        return false;

    AutoPtr<Stream> stream = OpenResource(resource->Name());
    return stream ? resource->Load(*stream) : false;
}

AutoPtr<Stream> ResourceCache::OpenResource(const std::string& nameIn)
{
    ZoneScoped;

    std::string name = SanitateResourceName(nameIn);
    AutoPtr<Stream> ret;

    for (size_t i = 0; i < resourceDirs.size(); ++i)
    {
        if (FileExists(resourceDirs[i] + name))
        {
            // Construct the file first with full path, then rename it to not contain the resource path,
            // so that the file's name can be used in further OpenResource() calls (for example over the network)
            ret = new File(resourceDirs[i] + name);
            break;
        }
    }

    // Fallback using absolute path
    if (!ret)
        ret = new File(name);

    if (!ret->IsReadable())
    {
        LOGERROR("Could not open resource file " + name);
        ret.Reset();
    }

    return ret;
}

Resource* ResourceCache::LoadResource(StringHash type, const std::string& nameIn)
{
    ZoneScoped;

    std::string name = SanitateResourceName(nameIn);

    // If empty name, return null pointer immediately without logging an error
    if (name.empty())
        return nullptr;

    // Check for existing resource
    auto key = std::make_pair(type, StringHash(name));
    auto it = resources.find(key);
    if (it != resources.end())
        return it->second;

    SharedPtr<Object> newObject = Create(type);
    if (!newObject)
    {
        LOGERROR("Could not load unknown resource type " + ToString(type));
        return nullptr;
    }
    Resource* newResource = dynamic_cast<Resource*>(newObject.Get());
    if (!newResource)
    {
        LOGERROR(Object::TypeNameFromType(type) + " is not a resource");
        return nullptr;
    }

    // Attempt to load the resource
    AutoPtr<Stream> stream = OpenResource(name);
    if (!stream)
        return nullptr;

    LOGDEBUG("Loading resource " + name);
    newResource->SetName(name);
    newResource->Load(*stream);
    // Store to cache
    resources[key] = newResource;
    return newResource;
}

void ResourceCache::ResourcesByType(std::vector<Resource*>& result, StringHash type) const
{
    result.clear();

    for (auto it = resources.begin(); it != resources.end(); ++it)
    {
        if (it->second->Type() == type)
            result.push_back(it->second);
    }
}

bool ResourceCache::Exists(const std::string& nameIn) const
{
    std::string name = SanitateResourceName(nameIn);

    for (size_t i = 0; i < resourceDirs.size(); ++i)
    {
        if (FileExists(resourceDirs[i] + name))
            return true;
    }

    // Fallback using absolute path
    return FileExists(name);
}

unsigned ResourceCache::LastModifiedTime(const std::string& nameIn) const
{
    std::string name = SanitateResourceName(nameIn);

    for (size_t i = 0; i < resourceDirs.size(); ++i)
    {
        if (FileExists(resourceDirs[i] + name))
            return ::LastModifiedTime(resourceDirs[i] + name);
    }

    // Fallback using absolute path
    return ::LastModifiedTime(name);
}

std::string ResourceCache::ResourceFileName(const std::string& name) const
{
    for (unsigned i = 0; i < resourceDirs.size(); ++i)
    {
        if (FileExists(resourceDirs[i] + name))
            return resourceDirs[i] + name;
    }

    return std::string();
}

std::string ResourceCache::SanitateResourceName(const std::string& nameIn) const
{
    // Sanitate unsupported constructs from the resource name
    std::string name = NormalizePath(nameIn);
    ReplaceInPlace(name, "../", "");
    ReplaceInPlace(name, "./", "");

    // If the path refers to one of the resource directories, normalize the resource name
    if (resourceDirs.size())
    {
        std::string namePath = Path(name);
        std::string exePath = ExecutableDir();
        for (unsigned i = 0; i < resourceDirs.size(); ++i)
        {
            std::string relativeResourcePath = resourceDirs[i];
            if (StartsWith(relativeResourcePath, exePath))
                relativeResourcePath = relativeResourcePath.substr(exePath.length());

            if (StartsWith(namePath, resourceDirs[i]))
                namePath = namePath.substr(resourceDirs[i].length());
            else if (StartsWith(namePath, relativeResourcePath))
                namePath = namePath.substr(relativeResourcePath.length());
        }

        name = namePath + FileNameAndExtension(name);
    }

    return Trim(name);
}

std::string ResourceCache::SanitateResourceDirName(const std::string& nameIn) const
{
    // Convert path to absolute
    std::string fixedPath = AddTrailingSlash(nameIn);
    if (!IsAbsolutePath(fixedPath))
        fixedPath = CurrentDir() + fixedPath;

    // Sanitate away /./ construct
    ReplaceInPlace(fixedPath, "/./", "/");

    return Trim(fixedPath);
}

void RegisterResourceLibrary()
{
    static bool registered = false;
    if (registered)
        return;

    Image::RegisterObject();
    JSONFile::RegisterObject();

    registered = true;
}
