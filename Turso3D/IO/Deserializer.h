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

/// Abstract stream for reading.
class TURSO3D_API Deserializer
{
public:
    /// Construct with zero size.
    Deserializer();
    /// Construct with defined size.
    Deserializer(size_t size);
    /// Destruct.
    virtual ~Deserializer();
    
    /// Read bytes from the stream. Return number of bytes actually read.
    virtual size_t Read(void* dest, size_t numBytes) = 0;
    /// Set position in bytes from the beginning of the stream.
    virtual size_t Seek(size_t position) = 0;
    /// Return current position in bytes.
    size_t Position() const { return position; }
    /// Return size in bytes.
    size_t Size() const { return size; }
    /// Return whether the end of stream has been reached.
    bool IsEof() const { return position >= size; }
    
    /// Read a variable-length encoded unsigned integer, which can use 29 bits maximum.
    unsigned ReadVLE();
    /// Read a text line.
    String ReadLine();
    /// Read a 4-character file ID.
    String ReadFileID();
    /// Read a byte buffer, with size prepended as a VLE value.
    Vector<unsigned char> ReadBuffer();
    
    /// Read a value, template version.
    template <class T> T Read()
    {
        T ret;
        Read(&ret, sizeof ret);
        return ret;
    }
    
protected:
    /// Stream position.
    size_t position;
    /// Stream size.
    size_t  size;
};

template<> bool Deserializer::Read();
template<> String Deserializer::Read();
template<> StringHash Deserializer::Read();
template<> ResourceRef Deserializer::Read();
template<> ResourceRefList Deserializer::Read();
template<> ObjectRef Deserializer::Read();
template<> JSONValue Deserializer::Read();

}
