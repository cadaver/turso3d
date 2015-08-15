// For conditions of distribution and use, see copyright notice in License.txt

#include "../Object/Object.h"
#include "ResourceRef.h"
#include "Stream.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

bool ResourceRef::FromString(const String& str)
{
    return FromString(str.CString());
}

bool ResourceRef::FromString(const char* str)
{
    Vector<String> values = String::Split(str, ';');
    if (values.Size() == 2)
    {
        type = values[0];
        name = values[1];
        return true;
    }
    else
        return false;
}

void ResourceRef::FromBinary(Stream& source)
{
    type = source.Read<StringHash>();
    name = source.Read<String>();
}

String ResourceRef::ToString() const
{
    return Object::TypeNameFromType(type) + ";" + name;
}

void ResourceRef::ToBinary(Stream& dest) const
{
    dest.Write(type);
    dest.Write(name);
}

bool ResourceRefList::FromString(const String& str)
{
    return FromString(str.CString());
}

bool ResourceRefList::FromString(const char* str)
{
    Vector<String> values = String::Split(str, ';');
    if (values.Size() >= 1)
    {
        type = values[0];
        names.Clear();
        for (size_t i = 1; i < values.Size(); ++i)
            names.Push(values[i]);
        return true;
    }
    else
        return false;
}

void ResourceRefList::FromBinary(Stream& source)
{
    type = source.Read<StringHash>();
    size_t num = source.ReadVLE();
    names.Clear();
    for (size_t i = 0; i < num && !source.IsEof(); ++i)
        names.Push(source.Read<String>());
}

String ResourceRefList::ToString() const
{
    String ret(Object::TypeNameFromType(type));
    for (auto it = names.Begin(); it != names.End(); ++it)
    {
        ret += ";";
        ret += *it;
    }
    return ret;
}

void ResourceRefList::ToBinary(Stream& dest) const
{
    dest.Write(type);
    dest.WriteVLE(names.Size());
    for (auto it = names.Begin(); it != names.End(); ++it)
        dest.Write(*it);
}

}
