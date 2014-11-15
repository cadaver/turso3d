// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Iterator.h"

#include <cassert>
#include <cstdarg>

namespace Turso3D
{

static const size_t CONVERSION_BUFFER_LENGTH = 256;

template <class T> class Vector;
class WString;

/// %String class.
class TURSO3D_API String
{
public:
    typedef RandomAccessIterator<char> Iterator;
    typedef RandomAccessConstIterator<char> ConstIterator;
    
    /// Construct empty.
    String() :
        buffer(nullptr)
    {
    }
    
    /// Copy-construct.
    String(const String& str) :
        buffer(nullptr)
    {
        *this = str;
    }
    
    /// Construct from a C string.
    String(const char* str) :
        buffer(nullptr)
    {
        *this = str;
    }
    
    /// Construct from a C string.
    String(char* str) :
        buffer(nullptr)
    {
        *this = (const char*)str;
    }
    
    /// Construct from a char array and length.
    String(const char* str, size_t numChars);
    /// Construct from a null-terminated wide character array.
    String(const wchar_t* str);
    /// Construct from a null-terminated wide character array.
    String(wchar_t* str);
    /// Construct from a wide character string.
    String(const WString& str);
    /// Construct from an integer.
    explicit String(int value);
    /// Construct from a short integer.
    explicit String(short value);
    /// Construct from a long integer.
    explicit String(long value);
    /// Construct from a long long integer.
    explicit String(long long value);
    /// Construct from an unsigned integer.
    explicit String(unsigned value);
    /// Construct from an unsigned short integer.
    explicit String(unsigned short value);
    /// Construct from an unsigned long integer.
    explicit String(unsigned long value);
    /// Construct from an unsigned long long integer.
    explicit String(unsigned long long value);
    /// Construct from a float.
    explicit String(float value);
    /// Construct from a double.
    explicit String(double value);
    /// Construct from a bool.
    explicit String(bool value);
    /// Construct from a character.
    explicit String(char value);
    /// Construct from a character and fill length.
    explicit String(char value, size_t numChars);
    
    /// Construct from a convertible value.
    template <class T> explicit String(const T& value) :
        buffer(nullptr)
    {
        *this = value.ToString();
    }
    
    /// Destruct.
    ~String();

    /// Assign a string.
    String& operator = (const String& rhs);
    /// Assign a C string.
    String& operator = (const char* rhs);
    /// Assign a C string.
    String& operator = (char* rhs);
    /// Add-assign a string.
    String& operator += (const String& rhs);
    /// Add-assign a C string.
    String& operator += (const char* rhs);
    /// Add-assign a C string.
    String& operator += (char* rhs);
    /// Add-assign a character.
    String& operator += (char rhs);
    /// Add-assign an integer.
    String& operator += (int rhs);
    /// Add-assign a short integer.
    String& operator += (short rhs);
    /// Add-assign a long integer.
    String& operator += (long rhs);
    /// Add-assign a long long integer.
    String& operator += (long long rhs);
    /// Add-assign an unsigned integer.
    String& operator += (unsigned rhs);
    /// Add-assign a short unsigned integer.
    String& operator += (unsigned short rhs);
    /// Add-assign an unsigned long integer.
    String& operator += (unsigned long rhs);
    /// Add-assign an unsigned long long integer.
    String& operator += (unsigned long long rhs);
    /// Add-assign a float.
    String& operator += (float rhs);
    /// Add-assign a float.
    String& operator += (double rhs);
    /// Add-assign a bool.
    String& operator += (bool rhs);
    /// Add-assign an arbitrary type.
    template <class T> String operator += (const T& rhs) { return *this += rhs.ToString(); }
    
    /// Add a string.
    String operator + (const String& rhs) const;
    /// Add a C string.
    String operator + (const char* rhs) const;
    /// Add a character.
    String operator + (char rhs) const;
    /// Test for equality with another string.
    bool operator == (const String& rhs) const { return Compare(rhs) == 0; }
    /// Test for inequality with another string.
    bool operator != (const String& rhs) const { return Compare(rhs) != 0; }
    /// Test if string is less than another string.
    bool operator < (const String& rhs) const { return Compare(rhs) < 0; }
    /// Test if string is greater than another string.
    bool operator > (const String& rhs) const { return Compare(rhs) > 0; }
    /// Test for equality with a C string.
    bool operator == (const char* rhs) const { return Compare(rhs) == 0; }
    /// Test for inequality with a C string.
    bool operator != (const char* rhs) const { return Compare(rhs) != 0; }
    /// Test if string is less than a C string.
    bool operator < (const char* rhs) const { return Compare(rhs) < 0; }
    /// Test if string is greater than a C string.
    bool operator > (const char* rhs) const { return Compare(rhs) > 0; }
    /// Return char at index.
    char& operator [] (size_t index) { assert(index < Length()); return Buffer()[index]; }
    /// Return const char at index.
    const char& operator [] (size_t index) const { assert(index < Length()); return Buffer()[index]; }
    
    /// Replace all occurrences of a character.
    void Replace(char replaceThis, char replaceWith, bool caseSensitive = true);
    /// Replace all occurrences of a string.
    void Replace(const String& replaceThis, const String& replaceWith, bool caseSensitive = true);
    /// Replace a substring.
    void Replace(size_t pos, size_t numChars, const String& replaceWith);
    /// Replace a substring with a C string.
    void Replace(size_t pos, size_t numChars, const char* replaceWith);
    /// Replace a substring by iterators.
    Iterator Replace(const Iterator& start, const Iterator& end, const String& replaceWith);
    /// Return a string with all occurrences of a character replaced.
    String Replaced(char replaceThis, char replaceWith, bool caseSensitive = true) const;
    /// Return a string with all occurrences of a string replaced.
    String Replaced(const String& replaceThis, const String& replaceWith, bool caseSensitive = true) const;
    /// Append a string.
    String& Append(const String& str);
    /// Append a C string.
    String& Append(const char* str);
    /// Append a character.
    String& Append(char c);
    /// Append characters.
    String& Append(const char* str, size_t numChars);
    /// Insert a string.
    void Insert(size_t pos, const String& str);
    /// Insert a character.
    void Insert(size_t pos, char c);
    /// Insert a string by iterator.
    Iterator Insert(const Iterator& dest, const String& str);
    /// Insert a string partially by iterators.
    Iterator Insert(const Iterator& dest, const Iterator& start, const Iterator& end);
    /// Insert a character by iterator.
    Iterator Insert(const Iterator& dest, char c);
    /// Erase a substring.
    void Erase(size_t pos, size_t numChars = 1);
    /// Erase a character by iterator.
    Iterator Erase(const Iterator& it);
    /// Erase a substring by iterators.
    Iterator Erase(const Iterator& start, const Iterator& end);
    /// Resize the string.
    void Resize(size_t newLength);
    /// Set new capacity.
    void Reserve(size_t newCapacity);
    /// Reallocate so that no extra memory is used.
    void Compact();
    /// Clear the string.
    void Clear();
    /// Swap with another string.
    void Swap(String& str);
    /// Append to string using formatting.
    String& AppendWithFormat(const char* formatStr, ... );
    /// Append to string using variable arguments.
    String& AppendWithFormatArgs(const char* formatStr, va_list args);
    
    /// Return char at index.
    char& At(size_t index) { assert(index < Length()); return Buffer()[index]; }
    /// Return const char at index.
    const char& At(size_t index) const { assert(index < Length()); return Buffer()[index]; }
    /// Return iterator to the beginning.
    Iterator Begin() { return Iterator(Buffer()); }
    /// Return const iterator to the beginning.
    ConstIterator Begin() const { return ConstIterator(Buffer()); }
    /// Return iterator to the end.
    Iterator End() { return Iterator(Buffer() + Length()); }
    /// Return const iterator to the end.
    ConstIterator End() const { return ConstIterator(Buffer() + Length()); }
    /// Return first char, or 0 if empty.
    char Front() const { return CString()[0]; }
    /// Return last char, or 0 if empty.
    char Back() const { return Length() ? CString()[Length()- 1] : CString()[0]; }
    /// Return a substring from position to end.
    String Substring(size_t pos) const;
    /// Return a substring with length from position.
    String Substring(size_t pos, size_t numChars) const;
    /// Return string with whitespace trimmed from the beginning and the end.
    String Trimmed() const;
    /// Return string in uppercase.
    String ToUpper() const;
    /// Return string in lowercase.
    String ToLower() const;
    /// Return substrings split by a separator char.
    Vector<String> Split(char separator) const;
    /// Return index to the first occurrence of a string, or NPOS if not found.
    size_t Find(const String& str, size_t startPos = 0, bool caseSensitive = true) const;
    /// Return index to the first occurrence of a character, or NPOS if not found.
    size_t Find(char c, size_t startPos = 0, bool caseSensitive = true) const;
    /// Return index to the last occurrence of a string, or NPOS if not found.
    size_t FindLast(const String& str, size_t startPos = NPOS, bool caseSensitive = true) const;
    /// Return index to the last occurrence of a character, or NPOS if not found.
    size_t FindLast(char c, size_t startPos = NPOS, bool caseSensitive = true) const;
    /// Return whether starts with a string.
    bool StartsWith(const String& str, bool caseSensitive = true) const;
    /// Return whether ends with a string.
    bool EndsWith(const String& str, bool caseSensitive = true) const;
    /// Return the C string.
    const char* CString() const { return buffer ? buffer + 2 * sizeof(size_t) : &endZero; }
    /// Return the char buffer.
    char* Buffer() const { return buffer ? buffer + 2 * sizeof(size_t) : &endZero; }
    /// Return number of characters in the string.
    size_t Length() const { return buffer ? reinterpret_cast<size_t*>(buffer)[0] : 0; }
    /// Return buffer capacity.
    size_t Capacity() const { return buffer ? reinterpret_cast<size_t*>(buffer)[1] : 0; }
    /// Return whether the string is zero characters long.
    bool IsEmpty() const { return Length() == 0; }
    /// Return comparision result with a string.
    int Compare(const String& str, bool caseSensitive = true) const { return Compare(CString(), str.CString(), caseSensitive); }
    /// Return comparision result with a C string.
    int Compare(const char* str, bool caseSensitive = true) const { return Compare(CString(), str, caseSensitive); }
    /// Return whether contains a specific occurence of a string.
    bool Contains(const String& str, bool caseSensitive = true) const { return Find(str, 0, caseSensitive) != NPOS; }
    /// Return whether contains a specific character.
    bool Contains(char c, bool caseSensitive = true) const { return Find(c, 0, caseSensitive) != NPOS; }
    /// Parse a bool.
    bool ToBool() const;
    /// Parse an integer.
    int ToInt() const;
    /// Parse an unsigned integer.
    unsigned ToUInt() const;
    /// Parse a float.
    float ToFloat() const;
    /// Return hash value for HashSet & HashMap.
    unsigned ToHash() const { return CaseSensitiveHash(buffer); }

    /// Construct UTF8 content from Latin1.
    void SetUTF8FromLatin1(const char* str);
    /// Construct UTF8 content from wide characters.
    void SetUTF8FromWChar(const wchar_t* str);
    /// Calculate number of characters in UTF8 content.
    size_t LengthUTF8() const;
    /// Return byte offset to char in UTF8 content.
    size_t ByteOffsetUTF8(size_t index) const;
    /// Return next Unicode character from UTF8 content and increase byte offset.
    unsigned NextUTF8Char(size_t& byteOffset) const;
    /// Return Unicode character at index from UTF8 content.
    unsigned AtUTF8(size_t index) const;
    /// Replace Unicode character at index from UTF8 content.
    void ReplaceUTF8(size_t index, unsigned unicodeChar);
    /// Append Unicode character at the end as UTF8.
    String& AppendUTF8(unsigned unicodeChar);
    /// Return a UTF8 substring from position to end.
    String SubstringUTF8(size_t pos) const;
    /// Return a UTF8 substring with length from position.
    String SubstringUTF8(size_t pos, size_t numChars) const;

    /// Parse a bool from the string. Is considered true if t/y/1 are found case-insensitively.
    static bool ToBool(const char* str);
    /// Parse an integer from the string.
    static int ToInt(const char* str);
    /// Parse an unsigned integer from the string.
    static unsigned ToUInt(const char* str);
    /// parse a float from the string.
    static float ToFloat(const char* str);
    /// Return the amount of substrings split by a separator char.
    static size_t CountElements(const char* str, char separator);
    /// Return substrings split by a separator char.
    static Vector<String> Split(const char* str, char separator);
    /// Return length of a C string.
    static size_t CStringLength(const char* str);
    /// Calculate a case-sensitive hash for a string.
    static unsigned CaseSensitiveHash(const char* str);
    /// Calculate a case-insensitive hash for a string.
    static unsigned CaseInsensitiveHash(const char* str);
    /// Compare two C strings.
    static int Compare(const char* str1, const char* str2, bool caseSensitive = true);
    /// Return an index to a string list corresponding to the given string, or a default value if not found. The string list must be empty-terminated.
    static size_t ListIndex(const String& value, const String* strings, size_t defaultIndex, bool caseSensitive = false);
    /// Return an index to a string list corresponding to the given C string, or a default value if not found. The string list must be empty-terminated.
    static size_t ListIndex(const char* value, const String* strings, size_t defaultIndex, bool caseSensitive = false);
    /// Return an index to a C string list corresponding to the given C string, or a default value if not found. The string list must be empty-terminated.
    static size_t ListIndex(const char* value, const char** strings, size_t defaultIndex, bool caseSensitive = false);
    /// Return a formatted string.
    static String Format(const char* formatString, ...);
    /// Encode Unicode character to UTF8. Pointer will be incremented.
    static void EncodeUTF8(char*& dest, unsigned unicodeChar);
    /// Decode Unicode character from UTF8. Pointer will be incremented.
    static unsigned DecodeUTF8(const char*& src);
    #ifdef WIN32
    /// Encode Unicode character to UTF16. Pointer will be incremented.
    static void EncodeUTF16(wchar_t*& dest, unsigned unicodeChar);
    /// Decode Unicode character from UTF16. Pointer will be incremented.
    static unsigned DecodeUTF16(const wchar_t*& src);
    #endif

    /// Position for "not found."
    static const size_t NPOS = (size_t)-1;
    /// Initial dynamic allocation size.
    static const size_t MIN_CAPACITY = 8;
    /// Empty string.
    static const String EMPTY;

private:
    /// Set new length.
    void SetLength(size_t length) { reinterpret_cast<size_t*>(buffer)[0] = length; }
    /// Set new capacity.
    void SetCapacity(size_t capacity) { reinterpret_cast<size_t*>(buffer)[1] = capacity; }
    /// Replace a substring with another substring.
    void Replace(size_t pos, size_t numChars, const char* srcStart, size_t srcLength);
    /// Move a range of characters within the string.
    void MoveRange(size_t dest, size_t src, size_t numChars);

    /// Copy chars from one buffer to another.
    static void CopyChars(char* dest, const char* src, size_t numChars);

    /// String buffer, null if not allocated. Contains length and capacity in the beginning.
    char* buffer;
    
    /// End zero for empty strings.
    static char endZero;
};

/// Add a string to a C string.
inline String operator + (const char* lhs, const String& rhs)
{
    String ret(lhs);
    ret += rhs;
    return ret;
}

/// Convert a char to uppercase.
inline char ToUpper(char c) { return (c >= 'a' && c <= 'z') ? c - 0x20 : c; }
/// Convert a char to lowercase.
inline char ToLower(char c) { return (c >= 'A' && c <= 'Z') ? c + 0x20 : c; }
/// Return whether a char is an alphabet letter.
inline bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
/// Return whether a char is a digit.
inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}
