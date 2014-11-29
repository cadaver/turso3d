// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/String.h"

namespace Turso3D
{

class JSONValue;
class StringHash;
template <class T> class Vector;
struct ObjectRef;
struct ResourceRef;
struct ResourceRefList;

/// Abstract stream for reading and writing.
class TURSO3D_API Stream
{
public:
    /// Construct with zero size.
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
    void SetName(const String& newName);
    /// Change the stream name.
    void SetName(const char* newName);
    /// Read a variable-length encoded unsigned integer, which can use 29 bits maximum.
    unsigned ReadVLE();
    /// Read a text line.
    String ReadLine();
    /// Read a 4-character file ID.
    String ReadFileID();
    /// Read a byte buffer, with size prepended as a VLE value.
    Vector<unsigned char> ReadBuffer();
    /// Write a string with optional null termination.
    void WriteString(const String& value, bool nullTerminate = true);
    /// Write a C string with optional null termination.
    void WriteString(const char* value, bool nullTerminate = true);
    /// Write a four-letter file ID. If the string is not long enough, spaces will be appended.
    void WriteFileID(const String& value);
    /// Write a byte buffer, with size encoded as VLE.
    void WriteBuffer(const Vector<unsigned char>& buffer);
    /// Write a variable-length encoded unsigned integer, which can use 29 bits maximum.
    void WriteVLE(size_t value);
    /// Write a text line. Char codes 13 & 10 will be automatically appended.
    void WriteLine(const String& value);

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
    const String& Name() const { return name; }
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
    String name;
};

template<> TURSO3D_API bool Stream::Read();
template<> TURSO3D_API String Stream::Read();
template<> TURSO3D_API StringHash Stream::Read();
template<> TURSO3D_API ResourceRef Stream::Read();
template<> TURSO3D_API ResourceRefList Stream::Read();
template<> TURSO3D_API ObjectRef Stream::Read();
template<> TURSO3D_API JSONValue Stream::Read();
template<> TURSO3D_API void Stream::Write(const bool& value);
template<> TURSO3D_API void Stream::Write(const String& value);
template<> TURSO3D_API void Stream::Write(const StringHash& value);
template<> TURSO3D_API void Stream::Write(const ResourceRef& value);
template<> TURSO3D_API void Stream::Write(const ResourceRefList& value);
template<> TURSO3D_API void Stream::Write(const ObjectRef& value);
template<> TURSO3D_API void Stream::Write(const JSONValue& value);

}
