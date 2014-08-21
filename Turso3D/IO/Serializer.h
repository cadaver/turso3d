// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

namespace Turso3D
{

class JSONValue;
class String;
class StringHash;
template <class T> class Vector;
struct ObjectRef;
struct ResourceRef;
struct ResourceRefList;

/// Abstract stream for writing.
class TURSO3D_API Serializer
{
public:
    /// Destruct.
    virtual ~Serializer();
    
    /// Write bytes to the stream. Return number of bytes actually written.
    virtual size_t Write(const void* data, size_t size) = 0;
    
    /// Write a string with optional null termination.
    void WriteString(const String& value, bool nullTerminate = true);
    /// Write a C string with optional null termination.
    void WriteString(const char* value, bool nullTerminate = true);
    /// Write a four-letter file ID. If the string is not long enough, spaces will be appended.
    void WriteFileID(const String& value);
    /// Write a byte buffer, with size encoded as VLE.
    void WriteBuffer(const PODVector<unsigned char>& buffer);
    /// Write a variable-length encoded unsigned integer, which can use 29 bits maximum.
    void WriteVLE(size_t value);
    /// Write a text line. Char codes 13 & 10 will be automatically appended.
    void WriteLine(const String& value);
    
    /// Write a value, template version.
    template <class T> void Write(const T& value) { Write(&value, sizeof value); }
};

template<> void Serializer::Write(const bool& value);
template<> void Serializer::Write(const String& value);
template<> void Serializer::Write(const StringHash& value);
template<> void Serializer::Write(const ResourceRef& value);
template<> void Serializer::Write(const ResourceRefList& value);
template<> void Serializer::Write(const ObjectRef& value);
template<> void Serializer::Write(const JSONValue& value);

}
