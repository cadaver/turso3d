// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "IntRect.h"

#include <cstdio>
#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const IntRect IntRect::ZERO(0, 0, 0, 0);

bool IntRect::FromString(const String& str)
{
    return FromString(str.CString());
}

bool IntRect::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 4)
        return false;
    
    char* ptr = (char*)str;
    left = strtol(ptr, &ptr, 10);
    top = strtol(ptr, &ptr, 10);
    right = strtol(ptr, &ptr, 10);
    bottom = strtol(ptr, &ptr, 10);
    
    return true;
}

String IntRect::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%d %d %d %d", left, top, right, bottom);
    return String(tempBuffer);
}

}
