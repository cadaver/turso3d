// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/Math.h"
#include "JSONValue.h"
#include "ResourceRef.h"
#include "Serializer.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Serializer::~Serializer()
{
}

void Serializer::WriteString(const String& value, bool nullTerminate)
{
    WriteString(value.CString(), nullTerminate);
}

void Serializer::WriteString(const char* value, bool nullTerminate)
{
    // Count length to the first zero, because ReadString() does the same
    size_t length = String::CStringLength(value);
    if (nullTerminate)
        Write(value, length + 1);
    else
        Write(value, length);
}

void Serializer::WriteFileID(const String& value)
{
    Write(value.CString(), Min((int)value.Length(), 4));
    for (size_t i = value.Length(); i < 4; ++i)
        Write(' ');
}

void Serializer::WriteBuffer(const Vector<unsigned char>& value)
{
    size_t numBytes = value.Size();
    
    WriteVLE(numBytes);
    if (numBytes)
        Write(&value[0], numBytes);
}

void Serializer::WriteVLE(size_t value)
{
    unsigned char data[4];
    
    if (value < 0x80)
        Write((unsigned char)value);
    else if (value < 0x4000)
    {
        data[0] = (unsigned char)value | 0x80;
        data[1] = (unsigned char)(value >> 7);
        Write(data, 2);
    }
    else if (value < 0x200000)
    {
        data[0] = (unsigned char)value | 0x80;
        data[1] = (unsigned char)((value >> 7) | 0x80);
        data[2] = (unsigned char)(value >> 14);
        Write(data, 3);
    }
    else
    {
        data[0] = (unsigned char)value | 0x80;
        data[1] = (unsigned char)((value >> 7) | 0x80);
        data[2] = (unsigned char)((value >> 14) | 0x80);
        data[3] = (unsigned char)(value >> 21);
        Write(data, 4);
    }
}

void Serializer::WriteLine(const String& value)
{
    Write(value.CString(), value.Length());
    Write('\r');
    Write('\n');
}

template<> void Serializer::Write<bool>(const bool& value)
{
    Write<unsigned char>(value ? 1 : 0);
}

template<> void Serializer::Write<String>(const String& value)
{
    WriteString(value);
}

template<> void Serializer::Write<StringHash>(const StringHash& value)
{
    Write(value.Value());
}

template<> void Serializer::Write<ResourceRef>(const ResourceRef& value)
{
    Write(value.type);
    Write(value.name);
}

template<> void Serializer::Write<ResourceRefList>(const ResourceRefList& value)
{
    Write(value.type);
    WriteVLE(value.names.Size());
    for (size_t i = 0; i < value.names.Size(); ++i)
        Write(value.names[i]);
}

template<> void Serializer::Write<JSONValue>(const JSONValue& value)
{
    Write((unsigned char)value.Type());
    
    switch (value.Type())
    {
    case JSON_BOOL:
        Write(value.GetBool());
        break;
        
    case JSON_NUMBER:
        Write(value.GetNumber());
        break;

    case JSON_STRING:
        Write(value.GetString());
        break;

    case JSON_ARRAY:
        {
            const JSONArray& array = value.GetArray();
            WriteVLE(array.Size());
            for (JSONArray::ConstIterator it = array.Begin(); it != array.End(); ++it)
                Write(*it);
        }
        break;
        
    case JSON_OBJECT:
        {
            const JSONObject& object = value.GetObject();
            WriteVLE(object.Size());
            for (JSONObject::ConstIterator it = object.Begin(); it != object.End(); ++it)
            {
                Write(it->first);
                Write(it->second);
            }
        }
        break;
        
    default:
        break;
    }
}

}
