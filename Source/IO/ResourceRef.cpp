// For conditions of distribution and use, see copyright notice in License.txt

#include "ResourceRef.h"
#include "Stream.h"
#include "StringUtils.h"
#include "../Object/Object.h"

bool ResourceRef::FromString(const std::string& str)
{
    return FromString(str.c_str());
}

bool ResourceRef::FromString(const char* string)
{
    std::vector<std::string> values = Split(string, ';');
    if (values.size() == 2)
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
    name = source.Read<std::string>();
}

std::string ResourceRef::ToString() const
{
    return Object::TypeNameFromType(type) + ";" + name;
}

void ResourceRef::ToBinary(Stream& dest) const
{
    dest.Write(type);
    dest.Write(name);
}

bool ResourceRefList::FromString(const std::string& str)
{
    return FromString(str.c_str());
}

bool ResourceRefList::FromString(const char* string)
{
    std::vector<std::string> values = Split(string, ';');
    if (values.size() >= 1)
    {
        type = values[0];
        names.clear();
        for (size_t i = 1; i < values.size(); ++i)
            names.push_back(values[i]);
        return true;
    }
    else
        return false;
}

void ResourceRefList::FromBinary(Stream& source)
{
    type = source.Read<StringHash>();
    size_t num = source.ReadVLE();
    names.clear();
    for (size_t i = 0; i < num && !source.IsEof(); ++i)
        names.push_back(source.Read<std::string>());
}

std::string ResourceRefList::ToString() const
{
    std::string ret(Object::TypeNameFromType(type));
    for (auto it = names.begin(); it != names.end(); ++it)
    {
        ret += ";";
        ret += *it;
    }
    return ret;
}

void ResourceRefList::ToBinary(Stream& dest) const
{
    dest.Write(type);
    dest.WriteVLE(names.size());
    for (auto it = names.begin(); it != names.end(); ++it)
        dest.Write(*it);
}
