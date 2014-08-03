// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/String.h"
#include "Vector3.h"

#include <cstdio>
#include <cstdlib>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const Vector3 Vector3::ZERO;
const Vector3 Vector3::LEFT(-1.0f, 0.0f, 0.0f);
const Vector3 Vector3::RIGHT(1.0f, 0.0f, 0.0f);
const Vector3 Vector3::UP(0.0f, 1.0f, 0.0f);
const Vector3 Vector3::DOWN(0.0f, -1.0f, 0.0f);
const Vector3 Vector3::FORWARD(0.0f, 0.0f, 1.0f);
const Vector3 Vector3::BACK(0.0f, 0.0f, -1.0f);
const Vector3 Vector3::ONE(1.0f, 1.0f, 1.0f);

bool Vector3::FromString(const String& str)
{
    return FromString(str.CString());
}

bool Vector3::FromString(const char* str)
{
    size_t elements = String::CountElements(str, ' ');
    if (elements < 3)
        return false;
    
    char* ptr = (char*)str;
    x = (float)strtod(ptr, &ptr);
    y = (float)strtod(ptr, &ptr);
    z = (float)strtod(ptr, &ptr);
    
    return true;
}

String Vector3::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%g %g %g", x, y, z);
    return String(tempBuffer);
}

}
