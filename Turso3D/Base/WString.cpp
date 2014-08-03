// For conditions of distribution and use, see copyright notice in License.txt

#include "String.h"
#include "WString.h"

#include <cstring>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

WString::WString() :
    length(0),
    buffer(0)
{
}

WString::WString(const String& str) :
    length(0),
    buffer(0)
{
    #ifdef WIN32
    size_t neededSize = 0;
    wchar_t temp[3];
    
    size_t byteOffset = 0;
    while (byteOffset < str.Length())
    {
        wchar_t* dest = temp;
        String::EncodeUTF16(dest, str.NextUTF8Char(byteOffset));
        neededSize += dest - temp;
    }
    
    Resize(neededSize);
    
    byteOffset = 0;
    wchar_t* dest = buffer;
    while (byteOffset < str.Length())
        String::EncodeUTF16(dest, str.NextUTF8Char(byteOffset));
    #else
    Resize(str.LengthUTF8());
    
    size_t byteOffset = 0;
    wchar_t* dest = buffer;
    while (byteOffset < str.Length())
        *dest++ = str.NextUTF8Char(byteOffset);
    #endif
}

WString::~WString()
{
    delete[] buffer;
}

void WString::Resize(size_t newLength)
{
    if (!newLength)
    {
        delete[] buffer;
        buffer = 0;
        length = 0;
    }
    else
    {
        wchar_t* newBuffer = new wchar_t[newLength + 1];
        if (buffer)
        {
            size_t copyLength = length < newLength ? length : newLength;
            memcpy(newBuffer, buffer, copyLength * sizeof(wchar_t));
            delete[] buffer;
        }
        newBuffer[newLength] = 0;
        buffer = newBuffer;
        length = newLength;
    }
}

}
