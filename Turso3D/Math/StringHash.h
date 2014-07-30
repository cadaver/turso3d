// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/String.h"

namespace Turso3D
{

/// 32-bit case-insensitive hash value for a string.
class TURSO3D_API StringHash
{
public:
    /// Construct with zero value.
    StringHash() :
        value(0)
    {
    }
    
    /// Copy-construct.
    StringHash(const StringHash& hash) :
        value(hash.value)
    {
    }
    
    /// Construct with an initial value.
    explicit StringHash(unsigned value_) :
        value(value_)
    {
    }
    
    /// Construct from a C string case-insensitively.
    StringHash(const char* str) :
        value(String::CaseInsensitiveHash(str))
    {
    }
    
    /// Construct from a string case-insensitively.
    StringHash(const String& str) :
        value(String::CaseInsensitiveHash(str.CString()))
    {
    }
    
    /// Assign from another hash.
    StringHash& operator = (const StringHash& rhs)
    {
        value = rhs.value;
        return *this;
    }
    
    /// Add a hash.
    StringHash operator + (const StringHash& rhs) const
    {
        StringHash ret;
        ret.value = value + rhs.value;
        return ret;
    }
    
    /// Add-assign a hash.
    StringHash& operator += (const StringHash& rhs)
    {
        value += rhs.value;
        return *this;
    }
    
    // Test for equality with another hash.
    bool operator == (const StringHash& rhs) const { return value == rhs.value; }
    /// Test for inequality with another hash.
    bool operator != (const StringHash& rhs) const { return value != rhs.value; }
    /// Test if less than another hash.
    bool operator < (const StringHash& rhs) const { return value < rhs.value; }
    /// Test if greater than another hash.
    bool operator > (const StringHash& rhs) const { return value > rhs.value; }
    /// Return true if nonzero hash value.
    operator bool () const { return value != 0; }
    /// Return hash value.
    unsigned Value() const { return value; }
    /// Return as string.
    String ToString() const;
    /// Return hash value for HashSet & HashMap.
    unsigned ToHash() const { return value; }
    
    /// Calculate hash value case-insensitively from a C string.
    static unsigned Calculate(const char* str);
    
    /// Zero hash.
    static const StringHash ZERO;
    
private:
    /// Hash value.
    unsigned value;
};

}
