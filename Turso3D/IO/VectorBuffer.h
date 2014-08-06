// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Deserializer.h"
#include "Serializer.h"

namespace Turso3D
{

/// Dynamically sized buffer that can be read and written to as a stream.
class TURSO3D_API VectorBuffer : public Deserializer, public Serializer
{
public:
    /// Construct an empty buffer.
    VectorBuffer();
    /// Construct from another buffer.
    VectorBuffer(const Vector<unsigned char>& data);
    /// Construct from a memory area.
    VectorBuffer(const void* data, size_t numBytes);
    /// Construct from a stream.
    VectorBuffer(Deserializer& source, size_t numBytes);
    
    /// Read bytes from the buffer. Return number of bytes actually read.
    virtual size_t Read(void* dest, size_t size);
    /// Set position in bytes from the beginning of the buffer.
    virtual size_t Seek(size_t newPosition);
    /// Write bytes to the buffer. Return number of bytes actually written.
    virtual size_t Write(const void* data, size_t size);
    
    /// Set data from another buffer.
    void SetData(const Vector<unsigned char>& data);
    /// Set data from a memory area.
    void SetData(const void* data, size_t numBytes);
    /// Set data from a stream.
    void SetData(Deserializer& source, size_t numBytes);
    /// Reset to zero size.
    void Clear();
    /// Set size.
    void Resize(size_t newSize);
    
    /// Return data.
    const unsigned char* Data() const { return buffer.Begin().ptr; }
    /// Return non-const data.
    unsigned char* ModifiableData() { return buffer.Begin().ptr; }
    /// Return the buffer.
    const Vector<unsigned char>& Buffer() const { return buffer; }
    
private:
    /// Dynamic data buffer.
    Vector<unsigned char> buffer;
};

}
