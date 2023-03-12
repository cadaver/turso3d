// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Stream.h"

/// %File open mode.
enum FileMode
{
    FILE_READ = 0,
    FILE_WRITE,
    FILE_READWRITE
};

/// Filesystem file.
class File : public Stream
{
public:
    /// Construct.
    File();
    /// Construct and open a file.
    File(const std::string& fileName, FileMode fileMode = FILE_READ);
    /// Destruct. Close the file if open.
    ~File();
    
    /// Read bytes from the file. Return number of bytes actually read.
    size_t Read(void* dest, size_t numBytes) override;
    /// Set position in bytes from the beginning of the file.
    size_t Seek(size_t newPosition) override;
    /// Write bytes to the file. Return number of bytes actually written.
    size_t Write(const void* data, size_t numBytes) override;
    /// Return whether read operations are allowed.
    bool IsReadable() const override;
    /// Return whether write operations are allowed.
    bool IsWritable() const override;

    /// Open a file. Return true on success.
    bool Open(const std::string& fileName, FileMode fileMode = FILE_READ);
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
    
    using Stream::Read;
    using Stream::Write;
    
private:
    /// Open mode.
    FileMode mode;
    /// File handle.
    void* handle;
    /// Synchronization needed before read -flag.
    bool readSyncNeeded;
    /// Synchronization needed before write -flag.
    bool writeSyncNeeded;
};
