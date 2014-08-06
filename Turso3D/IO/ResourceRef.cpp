// For conditions of distribution and use, see copyright notice in License.txt

#include "../Object/Object.h"
#include "ResourceRef.h"

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

String ResourceRef::ToString() const
{
    return TypeNameFromType(type) + ";" + name;
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

String ResourceRefList::ToString() const
{
    String ret(TypeNameFromType(type));
    for (size_t i = 0; i < names.Size(); ++i)
    {
        ret += ";";
        ret += names[i];
    }
    return ret;
}

}
