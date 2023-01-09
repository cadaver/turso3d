// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <string>
#include <vector>

/// Return files.
static const unsigned SCAN_FILES = 0x1;
/// Return directories.
static const unsigned SCAN_DIRS = 0x2;
/// Return also hidden files.
static const unsigned SCAN_HIDDEN = 0x4;

/// Set the current working directory.
bool SetCurrentDir(const std::string& pathName);
/// Create a directory.
bool CreateDir(const std::string& pathName);
/// Rename a file. Return true on success.
bool RenameFile(const std::string& srcFileName, const std::string& destFileName);
/// Delete a file. Return true on success.
bool DeleteFile(const std::string& fileName);
/// Return the absolute current working directory.
std::string CurrentDir();
/// Return the file's last modified time as seconds since epoch, or 0 if can not be accessed.
unsigned LastModifiedTime(const std::string& fileName);
/// Set the file's last modified time as seconds since epoch. Return true on success.
bool SetLastModifiedTime(const std::string& fileName, unsigned newTime);
/// Check if a file exists.
bool FileExists(const std::string& fileName);
/// Check if a directory exists.
bool DirExists(const std::string& pathName);
/// Scan a directory for specified files.
void ScanDir(std::vector<std::string>& result, const std::string& pathName, const std::string& filter, unsigned flags = SCAN_FILES, bool recursive = false);
/// Return the executable's directory.
std::string ExecutableDir();
/// Split a full path to path, filename and extension.
void SplitPath(const std::string& fullPath, std::string& pathName, std::string& fileName, std::string& extension, bool lowercaseExtension = false);
/// Return the path from a full path.
std::string Path(const std::string& fullPath);
/// Return the filename from a full path.
std::string FileName(const std::string& fullPath);
/// Return the path and filename from a full path.
std::string PathAndFileName(const std::string& fullPath);
/// Return the extension from a full path.
std::string Extension(const std::string& fullPath, bool lowercaseExtension = false);
/// Return the filename and extension from a full path. The case of the extension is preserved by default, so that the file can be opened in case-sensitive operating systems.
std::string FileNameAndExtension(const std::string& fullPath, bool lowercaseExtension = false);
/// Replace the extension of a file name with another.
std::string ReplaceExtension(const std::string& fullPath, const std::string& newExtension);
/// Add a slash at the end of the path if missing and convert to normalized format (use slashes.)
std::string AddTrailingSlash(const std::string& pathName);
/// Remove the slash from the end of a path if exists and convert to normalized format (use slashes.)
std::string RemoveTrailingSlash(const std::string& pathName);
/// Return the parent path, or the path itself if not available.
std::string ParentPath(const std::string& pathName);
/// Convert a path to normalized (internal) format which uses slashes.
std::string NormalizePath(const std::string& pathName);
/// Convert a path to the format required by the operating system.
std::string NativePath(const std::string& pathName);
/// Return whether a path is absolute.
bool IsAbsolutePath(const std::string& pathName);
