// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Deserializer.h"
#include "Serializer.h"

namespace Turso3D
{

/// Memory area that can be read and written to as a stream.
class TURSO3D_API MemoryBuffer : public Deserializer, public Serializer
{
public:
    /// Construct with a pointer and size.
    MemoryBuffer(void* data, size_t numBytes);
    /// Construct as read-only with a pointer and size.
    MemoryBuffer(const void* data, size_t numBytes);
    /// Construct from a vector, which must not go out of scope before MemoryBuffer.
    MemoryBuffer(PODVector<unsigned char>& data);
    /// Construct from a read-only vector, which must not go out of scope before MemoryBuffer.
    MemoryBuffer(const PODVector<unsigned char>& data);
    
    /// Read bytes from the memory area. Return number of bytes actually read.
    virtual size_t Read(void* dest, size_t numBytes);
    /// Set position in bytes from the beginning of the memory area.
    virtual size_t Seek(size_t newPosition);
    /// Write bytes to the memory area.
    virtual size_t Write(const void* data, size_t numBytes);
    
    /// Return memory area.
    unsigned char* Data() { return buffer; }
    /// Return whether buffer is read-only.
    bool IsReadOnly() { return readOnly; }
    
    using Deserializer::Read;
    using Serializer::Write;
    
private:
    /// Pointer to the memory area.
    unsigned char* buffer;
    /// Read-only flag.
    bool readOnly;
};

}
