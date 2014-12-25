// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "Vector2.h"

#include <cstdio>
#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const Vector2 Vector2::ZERO(0.0f, 0.0f);
const Vector2 Vector2::LEFT(-1.0f, 0.0f);
const Vector2 Vector2::RIGHT(1.0f, 0.0f);
const Vector2 Vector2::UP(0.0f, 1.0f);
const Vector2 Vector2::DOWN(0.0f, -1.0f);
const Vector2 Vector2::ONE(1.0f, 1.0f);

bool Vector2::FromString(const String& str)
{
    return FromString(str.CString());
}

bool Vector2::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 2)
        return false;
    
    char* ptr = (char*)str;
    x = (float)strtod(ptr, &ptr);
    y = (float)strtod(ptr, &ptr);

    return true;
}

String Vector2::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g", x, y);
    return String(tempBuffer);
}

}
