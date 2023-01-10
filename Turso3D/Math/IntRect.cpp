// For conditions of distribution and use, see copyright notice in License.txt

#include "IntRect.h"
#include "../IO/StringUtils.h"

#include <cstdio>
#include <cstdlib>

const IntRect IntRect::ZERO(0, 0, 0, 0);

bool IntRect::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 4)
        return false;

    char* ptr = const_cast<char*>(string);
    left = strtol(ptr, &ptr, 10);
    top = strtol(ptr, &ptr, 10);
    right = strtol(ptr, &ptr, 10);
    bottom = strtol(ptr, &ptr, 10);
    
    return true;
}

std::string IntRect::ToString() const
{
    return FormatString("%d %d %d %d", left, top, right, bottom);
}
