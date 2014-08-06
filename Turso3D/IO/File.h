// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Deserializer.h"
#include "Serializer.h"

namespace Turso3D
{

/// %File open mode.
enum FileMode
{
    FILE_READ = 0,
    FILE_WRITE,
    FILE_READWRITE
};

class PackageFile;

/// Filesystem file.
class TURSO3D_API File : public Deserializer, public Serializer
{
public:
    /// Construct.
    File();
    /// Construct and open a file.
    File(const String& fileName, FileMode fileMode = FILE_READ);
    /// Destruct. Close the file if open.
    virtual ~File();
    
    /// Read bytes from the file. Return number of bytes actually read.
    virtual size_t Read(void* dest, size_t numBytes);
    /// Set position in bytes from the beginning of the file.
    virtual size_t Seek(size_t newPosition);
    /// Write bytes to the file. Return number of bytes actually written.
    virtual size_t Write(const void* data, size_t numBytes);
    /// Return the file path and name.
    virtual const String& Name() const { return name; }
    
    /// Open a file. Return true on success.
    bool Open(const String& fileName, FileMode fileMode = FILE_READ);
    /// Close the file.
    void Close();
    /// Flush any buffered output to the file.
    void Flush();
    
    /// Return the open mode.
    FileMode Mode() const { return mode; }
    /// Return whether is open.
    bool IsOpen() const;
    /// Return the file handle.
    void* Handle() const { return handle; }

private:
    /// File path and name.
    String name;
    /// Open mode.
    FileMode mode;
    /// File handle.
    void* handle;
    /// Synchronization needed before read -flag.
    bool readSyncNeeded;
    /// Synchronization needed before write -flag.
    bool writeSyncNeeded;
};

}
