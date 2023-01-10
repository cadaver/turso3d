// For conditions of distribution and use, see copyright notice in License.txt

#include "IntVector2.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const IntVector2 IntVector2::ZERO(0, 0);

bool IntVector2::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 2)
        return false;

    char* ptr = const_cast<char*>(string);
    x = strtol(ptr, &ptr, 10);
    y = strtol(ptr, &ptr, 10);
    
    return true;
}

std::string IntVector2::ToString() const
{
    return FormatString("%d %d", x, y);
}

