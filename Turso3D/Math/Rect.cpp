// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "../Base/Swap.h"
#include "Rect.h"

#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

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
        Swap(min.x, max.x);
    if (min.y > max.y)
        Swap(min.y, max.y);
}

bool Rect::FromString(const String& str)
{
    return FromString(str.CString());
}

bool Rect::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 4)
        return false;
    
    char* ptr = (char*)str;
    min.x = (float)strtod(ptr, &ptr);
    min.y = (float)strtod(ptr, &ptr);
    max.x = (float)strtod(ptr, &ptr);
    max.y = (float)strtod(ptr, &ptr);
    
    return true;
}

String Rect::ToString() const
{
    return min.ToString() + " " + max.ToString();
}

}
