// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <cassert>
#include <cstdarg>

namespace Turso3D
{

class String;

/// Wide character string. Only meant for converting from String and passing to the operating system where necessary.
class TURSO3D_API WString
{
public:
    /// Construct empty.
    WString();
    /// Construct from a string.
    WString(const String& str);
    /// Destruct.
    ~WString();
    
    /// Return char at index.
    wchar_t& operator [] (size_t index) { assert(index < length); return buffer[index]; }
    /// Return const char at index.
    const wchar_t& operator [] (size_t index) const { assert(index < length); return buffer[index]; }
    /// Return char at index.
    wchar_t& At(size_t index) { assert(index < length); return buffer[index]; }
    /// Return const char at index.
    const wchar_t& At(size_t index) const { assert(index < length); return buffer[index]; }
    /// Resize the string.
    void Resize(size_t newLength);
    /// Return whether the string is zero characters long.
    bool IsEmpty() const { return length == 0; }
    /// Return number of characters.
    size_t Length() const { return length; }
    /// Return character data.
    const wchar_t* CString() const { return buffer; }
    
private:
    /// String length.
    size_t length;
    /// String buffer, null if not allocated.
    wchar_t* buffer;
};

}
