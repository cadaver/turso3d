// For conditions of distribution and use, see copyright notice in License.txt

#include "Deserializer.h"
#include "JSONValue.h"
#include "ObjectRef.h"
#include "ResourceRef.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Deserializer::Deserializer() :
    position(0),
    size(0)
{
}

Deserializer::Deserializer(size_t size) :
    position(0),
    size(size)
{
}

Deserializer::~Deserializer()
{
}

unsigned Deserializer::ReadVLE()
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

String Deserializer::ReadLine()
{
    String ret;
    
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

String Deserializer::ReadFileID()
{
    String ret;
    ret.Resize(4);
    Read(&ret[0], 4);
    return ret;
}

PODVector<unsigned char> Deserializer::ReadBuffer()
{
    PODVector<unsigned char> ret(ReadVLE());
    if (ret.Size())
        Read(&ret[0], ret.Size());
    return ret;
}

template<> bool Deserializer::Read<bool>()
{
    return Read<unsigned char>() != 0;
}

template<> String Deserializer::Read<String>()
{
    String ret;
    
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

template<> StringHash Deserializer::Read<StringHash>()
{
    return StringHash(Read<unsigned>());
}

template<> ResourceRef Deserializer::Read<ResourceRef>()
{
    ResourceRef ret;
    ret.type = Read<StringHash>();
    ret.name = Read<String>();
    return ret;
}

template<> ResourceRefList Deserializer::Read<ResourceRefList>()
{
    ResourceRefList ret;
    ret.type = Read<StringHash>();
    ret.names.Resize(ReadVLE());
    for (Vector<String>::Iterator it = ret.names.Begin(); it != ret.names.End(); ++it)
        *it = Read<String>();
    return ret;
}

template<> ObjectRef Deserializer::Read<ObjectRef>()
{
    ObjectRef ret;
    ret.id = Read<unsigned>();
    return ret;
}

template<> JSONValue Deserializer::Read<JSONValue>()
{
    JSONValue ret;
    
    JSONType type = (JSONType)Read<unsigned char>();
    
    switch (type)
    {
    case JSON_BOOL:
        ret = Read<bool>();
        break;
        
    case JSON_NUMBER:
        ret = Read<double>();
        break;
        
    case JSON_STRING:
        ret = Read<String>();
        break;
        
    case JSON_ARRAY:
        {
            size_t num = ReadVLE();
            ret.SetEmptyArray();
            for (size_t i = 0; i < num; ++i)
                ret.Push(Read<JSONValue>());
        }
        break;
        
    case JSON_OBJECT:
        {
            size_t num = ReadVLE();
            ret.SetEmptyObject();
            for (size_t i = 0; i < num; ++i)
            {
                String key = Read<String>();
                ret[key] = Read<JSONValue>();
            }
        }
        break;
        
    default:
        break;
    }
    
    return ret;
}

}
