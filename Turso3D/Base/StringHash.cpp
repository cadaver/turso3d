// For conditions of distribution and use, see copyright notice in License.txt

#include "StringHash.h"

#include <cstdio>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

const StringHash StringHash::ZERO;

String StringHash::ToString() const
{
    char tempBuffer[CONVERSION_BUFFER_LENGTH];
    sprintf(tempBuffer, "%08X", value);
    return String(tempBuffer);
}

}
