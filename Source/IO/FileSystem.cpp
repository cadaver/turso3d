// For conditions of distribution and use, see copyright notice in License.txt

#include "FileSystem.h"
#include "StringUtils.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#include <sys/types.h>
#include <sys/utime.h>
#else
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <utime.h>
#include <sys/wait.h>
#define MAX_PATH 256
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

bool SetCurrentDir(const std::string& pathName)
{
    #ifdef _WIN32
    if (SetCurrentDirectory(NativePath(pathName).c_str()) == FALSE)
        return false;
    #else
    if (chdir(NativePath(pathName).c_str()) != 0)
        return false;
    #endif

    return true;
}

bool CreateDir(const std::string& pathName)
{
    #ifdef _WIN32
    bool success = (CreateDirectory(NativePath(RemoveTrailingSlash(pathName)).c_str(), 0) == TRUE) ||
        (GetLastError() == ERROR_ALREADY_EXISTS);
    #else
    bool success = mkdir(NativePath(RemoveTrailingSlash(pathName)).c_str(), S_IRWXU) == 0 || errno == EEXIST;
    #endif

    return success;
}

bool RenameFile(const std::string& srcFileName, const std::string& destFileName)
{
    #ifdef _WIN32
    return MoveFile(NativePath(srcFileName).c_str(), NativePath(destFileName).c_str()) != 0;
    #else
    return rename(NativePath(srcFileName).c_str(), NativePath(destFileName).c_str()) == 0;
    #endif
}

bool DeleteFile(const std::string& fileName)
{
    #ifdef _WIN32
    return DeleteFile(NativePath(fileName).c_str()) != 0;
    #else
    return remove(NativePath(fileName).c_str()) == 0;
    #endif
}

std::string CurrentDir()
{
    #ifdef _WIN32
    char path[MAX_PATH];
    path[0] = 0;
    GetCurrentDirectory(MAX_PATH, path);
    return AddTrailingSlash(std::string(path));
    #else
    char path[MAX_PATH];
    path[0] = 0;
    getcwd(path, MAX_PATH);
    return AddTrailingSlash(std::string(path));
    #endif
}

unsigned LastModifiedTime(const std::string& fileName)
{
    if (fileName.empty())
        return 0;

    #ifdef _WIN32
    struct _stat st;
    if (!_stat(fileName.c_str(), &st))
        return (unsigned)st.st_mtime;
    else
        return 0;
    #else
    struct stat st;
    if (!stat(fileName.c_str(), &st))
        return (unsigned)st.st_mtime;
    else
        return 0;
    #endif
}

bool SetLastModifiedTime(const std::string& fileName, unsigned newTime)
{
    if (fileName.empty())
        return false;

    #ifdef WIN32
    struct _stat oldTime;
    struct _utimbuf newTimes;
    if (_stat(fileName.c_str(), &oldTime) != 0)
        return false;
    newTimes.actime = oldTime.st_atime;
    newTimes.modtime = newTime;
    return _utime(fileName.c_str(), &newTimes) == 0;
    #else
    struct stat oldTime;
    struct utimbuf newTimes;
    if (stat(fileName.c_str(), &oldTime) != 0)
        return false;
    newTimes.actime = oldTime.st_atime;
    newTimes.modtime = newTime;
    return utime(fileName.c_str(), &newTimes) == 0;
    #endif
}

bool FileExists(const std::string& fileName)
{
    std::string fixedName = NativePath(RemoveTrailingSlash(fileName));

    #ifdef _WIN32
    DWORD attributes = GetFileAttributes(fixedName.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || attributes & FILE_ATTRIBUTE_DIRECTORY)
        return false;
    #else
    struct stat st;
    if (stat(fixedName.c_str(), &st) || st.st_mode & S_IFDIR)
        return false;
    #endif

    return true;
}

bool DirExists(const std::string& pathName)
{
    #ifndef WIN32
    // Always return true for the root directory
    if (pathName == "/")
        return true;
    #endif

    std::string fixedName = NativePath(RemoveTrailingSlash(pathName));

    #ifdef _WIN32
    DWORD attributes = GetFileAttributes(fixedName.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    #else
    struct stat st;
    if (stat(fixedName.c_str(), &st) || !(st.st_mode & S_IFDIR))
        return false;
    #endif

    return true;
}

static void ScanDirInternal(std::vector<std::string>& result, std::string path, const std::string& startPath,
    const std::string& filter, unsigned flags, bool recursive)
{
    path = AddTrailingSlash(path);
    std::string deltaPath;
    if (path.length() > startPath.length())
        deltaPath = path.substr(startPath.length());

    std::string filterExtension = filter.substr(filter.find('.'));
    if (filterExtension.find('*') != std::string::npos)
        filterExtension.clear();

#ifdef _WIN32
    WIN32_FIND_DATA info;
    HANDLE handle = FindFirstFile(std::string(path + "*").c_str(), &info);
    if (handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::string fileName(info.cFileName);
            if (!fileName.empty())
            {
                if (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN && !(flags & SCAN_HIDDEN))
                    continue;
                if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (flags & SCAN_DIRS)
                        result.push_back(deltaPath + fileName);
                    if (recursive && fileName != "." && fileName != "..")
                        ScanDirInternal(result, path + fileName, startPath, filter, flags, recursive);
                }
                else if (flags & SCAN_FILES)
                {
                    if (filterExtension.empty() || EndsWith(fileName, filterExtension))
                        result.push_back(deltaPath + fileName);
                }
            }
        } while (FindNextFile(handle, &info));

        FindClose(handle);
    }
#else
    DIR *dir;
    struct dirent *de;
    struct stat st;
    dir = opendir(NativePath(path).c_str());
    if (dir)
    {
        while ((de = readdir(dir)))
        {
            /// \todo Filename may be unnormalized Unicode on Mac OS X. Re-normalize as necessary
            std::string fileName(de->d_name);
            bool normalEntry = fileName != "." && fileName != "..";
            if (normalEntry && !(flags & SCAN_HIDDEN) && StartsWith(fileName, "."))
                continue;
            std::string pathAndName = path + fileName;
            if (!stat(pathAndName.c_str(), &st))
            {
                if (st.st_mode & S_IFDIR)
                {
                    if (flags & SCAN_DIRS)
                        result.push_back(deltaPath + fileName);
                    if (recursive && normalEntry)
                        ScanDirInternal(result, path + fileName, startPath, filter, flags, recursive);
                }
                else if (flags & SCAN_FILES)
                {
                    if (filterExtension.empty() || EndsWith(fileName, filterExtension))
                        result.push_back(deltaPath + fileName);
                }
            }
        }
        closedir(dir);
    }
#endif
}

void ScanDir(std::vector<std::string>& result, const std::string& pathName, const std::string& filter, unsigned flags, bool recursive)
{
    std::string initialPath = AddTrailingSlash(pathName);
    ScanDirInternal(result, initialPath, initialPath, filter, flags, recursive);
}

std::string ExecutableDir()
{
    std::string ret;

    #if defined(_WIN32)
    char exeName[MAX_PATH];
    exeName[0] = 0;
    GetModuleFileName(0, exeName, MAX_PATH);
    ret = Path(std::string(exeName));
    #elif defined(__APPLE__)
    char exeName[MAX_PATH];
    memset(exeName, 0, MAX_PATH);
    unsigned size = MAX_PATH;
    _NSGetExecutablePath(exeName, &size);
    ret = Path(std::string(exeName));
    #elif defined(__linux__)
    char exeName[MAX_PATH];
    memset(exeName, 0, MAX_PATH);
    pid_t pid = getpid();
    std::string link = "/proc/" + ToString(pid) + "/exe";
    readlink(link.c_str(), exeName, MAX_PATH);
    ret = Path(std::string(exeName));
    #endif
    
    // Sanitate /./ construct away
    ReplaceInPlace(ret, "/./", "/");
    return ret;
}

void SplitPath(const std::string& fullPath, std::string& pathName, std::string& fileName, std::string& extension, bool lowercaseExtension)
{
    std::string fullPathCopy = NormalizePath(fullPath);

    size_t extPos = fullPathCopy.find_last_of('.');
    size_t pathPos = fullPathCopy.find_last_of('/');

    if (extPos != std::string::npos && (pathPos == std::string::npos || extPos > pathPos))
    {
        extension = fullPathCopy.substr(extPos);
        if (lowercaseExtension)
            extension = ToLower(extension);
        fullPathCopy = fullPathCopy.substr(0, extPos);
    }
    else
        extension.clear();

    pathPos = fullPathCopy.find_last_of('/');
    if (pathPos != std::string::npos)
    {
        fileName = fullPathCopy.substr(pathPos + 1);
        pathName = fullPathCopy.substr(0, pathPos + 1);
    }
    else
    {
        fileName = fullPathCopy;
        pathName.clear();
    }
}

std::string Path(const std::string& fullPath)
{
    std::string path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return path;
}

std::string FileName(const std::string& fullPath)
{
    std::string path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return file;
}

std::string PathAndFileName(const std::string& fullPath)
{
    std::string path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return path + file;
}

std::string Extension(const std::string& fullPath, bool lowercaseExtension)
{
    std::string path, file, extension;
    SplitPath(fullPath, path, file, extension, lowercaseExtension);
    return extension;
}

std::string FileNameAndExtension(const std::string& fileName, bool lowercaseExtension)
{
    std::string path, file, extension;
    SplitPath(fileName, path, file, extension, lowercaseExtension);
    return file + extension;
}

std::string ReplaceExtension(const std::string& fullPath, const std::string& newExtension)
{
    std::string path, file, extension;
    SplitPath(fullPath, path, file, extension);
    return path + file + newExtension;
}

std::string AddTrailingSlash(const std::string& pathName)
{
    std::string ret = Trim(pathName);
    ReplaceInPlace(ret, '\\', '/');
    if (!ret.empty() && ret.back() != '/')
        ret += '/';
    return ret;
}

std::string RemoveTrailingSlash(const std::string& pathName)
{
    std::string ret = Trim(pathName);
    ReplaceInPlace(ret, '\\', '/');
    if (!ret.empty() && ret.back() == '/')
        ret.resize(ret.length() - 1);
    return ret;
}

std::string ParentPath(const std::string& path)
{
    size_t pos = RemoveTrailingSlash(path).find_last_of('/');
    if (pos != std::string::npos)
        return path.substr(0, pos + 1);
    else
        return std::string();
}

std::string NormalizePath(const std::string& pathName)
{
    std::string ret(pathName);
    ReplaceInPlace(ret, '\\', '/');
    return ret;
}

std::string NativePath(const std::string& pathName)
{
    std::string ret(pathName);
#ifdef _WIN32
    ReplaceInPlace(ret, '/', '\\');
#endif
    return ret;
}

bool IsAbsolutePath(const std::string& pathName)
{
    if (pathName.empty())
        return false;
    
    std::string path = NormalizePath(pathName);
    
    if (path[0] == '/')
        return true;
    
#ifdef _WIN32
    if (path.length() > 1 && isalpha(path[0]) && path[1] == ':')
        return true;
#endif

    return false;
}
