// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <string>
#include <vector>

class JSONValue;
class StringHash;
struct ObjectRef;
struct ResourceRef;
struct ResourceRefList;

/// Abstract stream for reading and writing.
class Stream
{
public:
    /// Default-construct with zero size.
    Stream();
    /// Construct with defined byte size.
    Stream(size_t numBytes);
    /// Destruct.
    virtual ~Stream();
    
    /// Read bytes from the stream. Return number of bytes actually read.
    virtual size_t Read(void* dest, size_t numBytes) = 0;
    /// Set position in bytes from the beginning of the stream. Return the position after the seek.
    virtual size_t Seek(size_t position) = 0;
    /// Write bytes to the stream. Return number of bytes actually written.
    virtual size_t Write(const void* data, size_t size) = 0;
    /// Return whether read operations are allowed.
    virtual bool IsReadable() const = 0;
    /// Return whether write operations are allowed.
    virtual bool IsWritable() const = 0;

    /// Change the stream name.
    void SetName(const std::string& newName);
    /// Change the stream name.
    void SetName(const char* newName);
    /// Read a variable-length encoded unsigned integer, which can use 29 bits maximum.
    unsigned ReadVLE();
    /// Read a text line.
    std::string ReadLine();
    /// Read a 4-character file ID.
    std::string ReadFileID();
    /// Read a byte buffer, with size prepended as a VLE value.
    std::vector<unsigned char> ReadBuffer();
    /// Write a four-letter file ID. If the string is not long enough, spaces will be appended.
    void WriteFileID(const std::string& value);
    /// Write a byte buffer, with size encoded as VLE.
    void WriteBuffer(const std::vector<unsigned char>& buffer);
    /// Write a variable-length encoded unsigned integer, which can use 29 bits maximum.
    void WriteVLE(size_t value);
    /// Write a text line. Char codes 13 & 10 will be automatically appended.
    void WriteLine(const std::string& value);

    /// Write a value, template version.
    template <class T> void Write(const T& value) { Write(&value, sizeof value); }

    /// Read a value, template version.
    template <class T> T Read()
    {
        T ret;
        Read(&ret, sizeof ret);
        return ret;
    }
    
    /// Return the stream name.
    const std::string& Name() const { return name; }
    /// Return current position in bytes.
    size_t Position() const { return position; }
    /// Return size in bytes.
    size_t Size() const { return size; }
    /// Return whether the end of stream has been reached.
    bool IsEof() const { return position >= size; }
    
protected:
    /// Stream position.
    size_t position;
    /// Stream size.
    size_t size;
    /// Stream name.
    std::string name;
};

/// Read a boolean.
template<> bool Stream::Read();
/// Read a string.
template<> std::string Stream::Read();
/// Read a string hash.
template<> StringHash Stream::Read();
/// Read a resource reference.
template<> ResourceRef Stream::Read();
/// Read a resource reference list.
template<> ResourceRefList Stream::Read();
/// Read an object reference.
template<> ObjectRef Stream::Read();
/// Read a JSON value.
template<> JSONValue Stream::Read();
/// Write a boolean.
template<> void Stream::Write(const bool& value);
/// Write a string.
template<> void Stream::Write(const std::string& value);
/// Write a string hash.
template<> void Stream::Write(const StringHash& value);
/// Write a resource reference.
template<> void Stream::Write(const ResourceRef& value);
/// Write a resource reference list.
template<> void Stream::Write(const ResourceRefList& value);
/// Write an object reference.
template<> void Stream::Write(const ObjectRef& value);
/// Write a JSON value.
template<> void Stream::Write(const JSONValue& value);
