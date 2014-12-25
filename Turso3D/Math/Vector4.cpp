// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "Vector4.h"

#include <cstdio>
#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const Vector4 Vector4::ZERO(0.0f, 0.0f, 0.0f, 0.0f);
const Vector4 Vector4::ONE(1.0f, 1.0f, 1.0f, 1.0f);

bool Vector4::FromString(const String& str)
{
    return FromString(str.CString());
}

bool Vector4::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 4)
        return false;
    
    char* ptr = (char*)str;
    x = (float)strtod(ptr, &ptr);
    y = (float)strtod(ptr, &ptr);
    z = (float)strtod(ptr, &ptr);
    w = (float)strtod(ptr, &ptr);
    
    return true;
}

String Vector4::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g %g", x, y, z, w);
    return String(tempBuffer);
}

}
