// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "IntVector2.h"

#include <cstdio>
#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const IntVector2 IntVector2::ZERO;

bool IntVector2::FromString(const String& str)
{
    return FromString(str.CString());
}

bool IntVector2::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 2)
        return false;
    
    char* ptr = (char*)str;
    x = strtol(ptr, &ptr, 10);
    y = strtol(ptr, &ptr, 10);
    
    return true;
}

String IntVector2::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%d %d", x, y);
    return String(tempBuffer);
}

}
