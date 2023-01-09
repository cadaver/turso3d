// For conditions of distribution and use, see copyright notice in License.txt

#include "StringHash.h"
#include "StringUtils.h"

#include <cctype>
#include <cstdio>

const StringHash StringHash::ZERO;

std::string StringHash::ToString() const
{
    return FormatString("%08X", value);
}

unsigned StringHash::Calculate(const char* str)
{
    unsigned hash = 0;
    while (*str)
    {
        hash = (tolower(*str)) + (hash << 6) + (hash << 16) - hash;
        ++str;
    }

    return hash;
}
