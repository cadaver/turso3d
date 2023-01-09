// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Stream.h"

/// Memory area that can be read and written to as a stream.
class MemoryBuffer : public Stream
{
public:
    /// Construct with a pointer and size.
    MemoryBuffer(void* data, size_t numBytes);
    /// Construct as read-only with a pointer and size.
    MemoryBuffer(const void* data, size_t numBytes);
    /// Construct from a vector, which must not go out of scope before MemoryBuffer.
    MemoryBuffer(std::vector<unsigned char>& data);
    /// Construct from a read-only vector, which must not go out of scope before MemoryBuffer.
    MemoryBuffer(const std::vector<unsigned char>& data);
    
    /// Read bytes from the memory area. Return number of bytes actually read.
    size_t Read(void* dest, size_t numBytes) override;
    /// Set position in bytes from the beginning of the memory area.
    size_t Seek(size_t newPosition) override;
    /// Write bytes to the memory area.
    size_t Write(const void* data, size_t numBytes) override;
    /// Return whether read operations are allowed.
    bool IsReadable() const override;
    /// Return whether write operations are allowed.
    bool IsWritable() const override;

    /// Return memory area.
    unsigned char* Data() { return buffer; }
    
    using Stream::Read;
    using Stream::Write;
    
private:
    /// Pointer to the memory area.
    unsigned char* buffer;
    /// Read-only flag.
    bool readOnly;
};
