// For conditions of distribution and use, see copyright notice in License.txt

#include "Rect.h"
#include "../IO/StringUtils.h"

#include <utility>
#include <cstdlib>

const Rect Rect::FULL(-1.0f, -1.0f, 1.0f, 1.0f);
const Rect Rect::POSITIVE(0.0f, 0.0f, 1.0f, 1.0f);
const Rect Rect::ZERO(0.0f, 0.0f, 0.0f, 0.0f);

void Rect::Clip(const Rect& rect)
{
    if (rect.min.x > min.x)
        min.x = rect.min.x;
    if (rect.max.x < max.x)
        max.x = rect.max.x;
    if (rect.min.y > min.y)
        min.y = rect.min.y;
    if (rect.max.y < max.y)
        max.y = rect.max.y;
    
    if (min.x > max.x)
        std::swap(min.x, max.x);
    if (min.y > max.y)
        std::swap(min.y, max.y);
}

bool Rect::FromString(const char* string)
{
    size_t elements = CountElements(string);
    if (elements < 4)
        return false;

    char* ptr = const_cast<char*>(string);
    min.x = (float)strtod(ptr, &ptr);
    min.y = (float)strtod(ptr, &ptr);
    max.x = (float)strtod(ptr, &ptr);
    max.y = (float)strtod(ptr, &ptr);
    
    return true;
}

std::string Rect::ToString() const
{
    return min.ToString() + " " + max.ToString();
}
