// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/Math.h"
#include "Stream.h"
#include "JSONValue.h"
#include "ObjectRef.h"
#include "ResourceRef.h"

Stream::Stream() :
    position(0),
    size(0)
{
}

Stream::Stream(size_t numBytes) :
    position(0),
    size(numBytes)
{
}

Stream::~Stream()
{
}

void Stream::SetName(const std::string& newName)
{
    name = newName;
}

void Stream::SetName(const char* newName)
{
    name = newName;
}

unsigned Stream::ReadVLE()
{
    unsigned ret;
    unsigned char byte;
    
    byte = Read<unsigned char>();
    ret = byte & 0x7f;
    if (byte < 0x80)
        return ret;
    
    byte = Read<unsigned char>();
    ret |= ((unsigned)(byte & 0x7f)) << 7;
    if (byte < 0x80)
        return ret;
    
    byte = Read<unsigned char>();
    ret |= ((unsigned)(byte & 0x7f)) << 14;
    if (byte < 0x80)
        return ret;
    
    byte = Read<unsigned char>();
    ret |= ((unsigned)byte) << 21;
    return ret;
}

std::string Stream::ReadLine()
{
    std::string ret;
    
    while (!IsEof())
    {
        char c = Read<char>();
        if (c == 10)
            break;
        if (c == 13)
        {
            // Peek next char to see if it's 10, and skip it too
            if (!IsEof())
            {
                char next = Read<char>();
                if (next != 10)
                    Seek(position - 1);
            }
            break;
        }
        
        ret += c;
    }
    
    return ret;
}

std::string Stream::ReadFileID()
{
    std::string ret;
    ret.resize(4);
    Read(&ret[0], 4);
    return ret;
}

std::vector<unsigned char> Stream::ReadBuffer()
{
    std::vector<unsigned char> ret(ReadVLE());
    if (ret.size())
        Read(&ret[0], ret.size());
    return ret;
}

template<> bool Stream::Read<bool>()
{
    return Read<unsigned char>() != 0;
}

template<> std::string Stream::Read<std::string>()
{
    std::string ret;
    
    while (!IsEof())
    {
        char c = Read<char>();
        if (!c)
            break;
        else
            ret += c;
    }
    
    return ret;
}

template<> StringHash Stream::Read<StringHash>()
{
    return StringHash(Read<unsigned>());
}

template<> ResourceRef Stream::Read<ResourceRef>()
{
    ResourceRef ret;
    ret.FromBinary(*this);
    return ret;
}

template<> ResourceRefList Stream::Read<ResourceRefList>()
{
    ResourceRefList ret;
    ret.FromBinary(*this);
    return ret;
}

template<> ObjectRef Stream::Read<ObjectRef>()
{
    ObjectRef ret;
    ret.id = Read<unsigned>();
    return ret;
}

template<> JSONValue Stream::Read<JSONValue>()
{
    JSONValue ret;
    ret.FromBinary(*this);
    return ret;
}

void Stream::WriteFileID(const std::string& value)
{
    Write(value.c_str(), Min(value.length(), (size_t)4));
    for (size_t i = value.length(); i < 4; ++i)
        Write(' ');
}

void Stream::WriteBuffer(const std::vector<unsigned char>& value)
{
    size_t numBytes = value.size();
    
    WriteVLE(numBytes);
    if (numBytes)
        Write(&value[0], numBytes);
}

void Stream::WriteVLE(size_t value)
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

void Stream::WriteLine(const std::string& value)
{
    Write(value.c_str(), value.length());
    Write('\r');
    Write('\n');
}

template<> void Stream::Write<bool>(const bool& value)
{
    Write<unsigned char>(value ? 1 : 0);
}

template<> void Stream::Write<std::string>(const std::string& value)
{
    // Write content and null terminator
    Write(value.c_str(), value.length() + 1);
}

template<> void Stream::Write<StringHash>(const StringHash& value)
{
    Write(value.Value());
}

template<> void Stream::Write<ResourceRef>(const ResourceRef& value)
{
    value.ToBinary(*this);
}

template<> void Stream::Write<ResourceRefList>(const ResourceRefList& value)
{
    value.ToBinary(*this);
}

template<> void Stream::Write<ObjectRef>(const ObjectRef& value)
{
    Write(value.id);
}

template<> void Stream::Write<JSONValue>(const JSONValue& value)
{
    value.ToBinary(*this);
}
