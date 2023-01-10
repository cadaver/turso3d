// For conditions of distribution and use, see copyright notice in License.txt

#include "IntVector3.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const IntVector3 IntVector3::ZERO(0, 0, 0);

bool IntVector3::FromString(const std::string& str)
{
    return FromString(str.c_str());
}

bool IntVector3::FromString(const char* str)
{
    size_t elements = CountElements(str, ' ');
    if (elements < 3)
        return false;
    
    char* ptr = (char*)str;
    x = strtol(ptr, &ptr, 10);
    y = strtol(ptr, &ptr, 10);
    z = strtol(ptr, &ptr, 10);

    return true;
}

std::string IntVector3::ToString() const
{
    return FormatString("%d %d %d", x, y, z);
}
