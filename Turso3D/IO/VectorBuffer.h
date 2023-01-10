// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Stream.h"

/// Dynamically sized buffer that can be read and written to as a stream.
class VectorBuffer : public Stream
{
public:
    /// Construct an empty buffer.
    VectorBuffer();
    /// Construct from another buffer.
    VectorBuffer(const std::vector<unsigned char>& data);
    /// Construct from a memory area.
    VectorBuffer(const void* data, size_t numBytes);
    /// Construct from a stream.
    VectorBuffer(Stream& source, size_t numBytes);
    
    /// Read bytes from the buffer. Return number of bytes actually read.
    size_t Read(void* dest, size_t size) override;
    /// Set position in bytes from the beginning of the buffer.
    size_t Seek(size_t newPosition) override;
    /// Write bytes to the buffer. Return number of bytes actually written.
    size_t Write(const void* data, size_t size) override;
    /// Return whether read operations are allowed.
    bool IsReadable() const override;
    /// Return whether write operations are allowed.
    bool IsWritable() const override;

    /// Set data from another buffer.
    void SetData(const std::vector<unsigned char>& data);
    /// Set data from a memory area.
    void SetData(const void* data, size_t numBytes);
    /// Set data from a stream.
    void SetData(Stream& source, size_t numBytes);
    /// Reset to zero size.
    void Clear();
    /// Set size.
    void Resize(size_t newSize);
    
    /// Return data.
    const unsigned char* Data() const { return &*buffer.begin(); }
    /// Return non-const data.
    unsigned char* ModifiableData() { return const_cast<unsigned char*>(&*buffer.begin()); }
    /// Return the buffer.
    const std::vector<unsigned char>& Buffer() const { return buffer; }
    
    using Stream::Read;
    using Stream::Write;
    
private:
    /// Dynamic data buffer.
    std::vector<unsigned char> buffer;
};
