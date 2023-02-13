// For conditions of distribution and use, see copyright notice in License.txt

#include "StringUtils.h"

#include <cstdarg>
#include <cstring>
#include <cctype>

size_t CountElements(const std::string& string, char separator)
{
    return CountElements(string.c_str(), separator);
}

size_t CountElements(const char* string, char separator)
{
    if (!string)
        return 0;

    const char* endPos = string + strlen(string);
    const char* pos = string;
    size_t ret = 0;
    
    while (pos < endPos)
    {
        if (*pos != separator)
            break;
        ++pos;
    }
    
    while (pos < endPos)
    {
        const char* start = pos;
        
        while (start < endPos)
        {
            if (*start == separator)
                break;
            
            ++start;
        }
        
        if (start == endPos)
        {
            ++ret;
            break;
        }

        const char* end = start;
        
        while (end < endPos)
        {
            if (*end != separator)
                break;
            
            ++end;
        }
        
        ++ret;
        pos = end;
    }
    
    return ret;
}

std::string Trim(const std::string& string)
{
    size_t trimStart = 0;
    size_t trimEnd = string.length();

    while (trimStart < trimEnd)
    {
        char c = string[trimStart];
        if (c != ' ' && c != 9)
            break;
        ++trimStart;
    }
    while (trimEnd > trimStart)
    {
        char c = string[trimEnd - 1];
        if (c != ' ' && c != 9)
            break;
        --trimEnd;
    }

    return string.substr(trimStart, trimEnd - trimStart);
}

std::string ToLower(const std::string& string)
{
    std::string ret(string);

    for (size_t i = 0; i < ret.length(); ++i)
        ret[i] = (char)tolower(ret[i]);

    return ret;
}

std::string ToUpper(const std::string& string)
{
    std::string ret(string);

    for (size_t i = 0; i < ret.length(); ++i)
        ret[i] = (char)toupper(ret[i]);

    return ret;
}

std::string Replace(const std::string& string, const std::string& replaceThis, const std::string& replaceWith)
{
    std::string ret(string);
    size_t idx = ret.find(replaceThis);

    while (idx != std::string::npos)
    {
        ret.replace(idx, replaceThis.length(), replaceWith);
        idx += replaceWith.length();
        idx = ret.find(replaceThis, idx);
    }

    return ret;
}

std::string Replace(const std::string& string, char replaceThis, char replaceWith)
{
    std::string ret(string);

    for (size_t i = 0; i < ret.length(); ++i)
    {
        if (ret[i] == replaceThis)
            ret[i] = replaceWith;
    }

    return ret;
}

void ReplaceInPlace(std::string& string, const std::string& replaceThis, const std::string& replaceWith)
{
    size_t idx = string.find(replaceThis);

    while (idx != std::string::npos)
    {
        string.replace(idx, replaceThis.length(), replaceWith);
        idx += replaceWith.length();
        idx = string.find(replaceThis, idx);
    }
}

void ReplaceInPlace(std::string& string, char replaceThis, char replaceWith)
{
    for (size_t i = 0; i < string.length(); ++i)
    {
        if (string[i] == replaceThis)
            string[i] = replaceWith;
    }
}

bool StartsWith(const std::string& string, const std::string& substring)
{
    return string.find(substring) == 0;
}

bool EndsWith(const std::string& string, const std::string& substring)
{
    return string.rfind(substring) == string.length() - substring.length();
}

std::vector<std::string> Split(const std::string& string, char separator)
{
    return Split(string.c_str(), separator);
}

std::vector<std::string> Split(const char* string, char separator)
{
    std::vector<std::string> ret;
    if (!string)
        return ret;

    size_t pos = 0;
    size_t length = strlen(string);

    while (pos < length)
    {
        if (string[pos] != separator)
            break;
        ++pos;
    }
    
    while (pos < length)
    {
        size_t start = pos;
        
        while (start < length)
        {
            if (string[start] == separator)
                break;
            
            ++start;
        }
        
        if (start == length)
        {
            ret.push_back(std::string(&string[pos]));
            break;
        }
        
        size_t end = start;
        
        while (end < length)
        {
            if (string[end] != separator)
                break;
            
            ++end;
        }
        
        ret.push_back(std::string(&string[pos], start - pos));
        pos = end;
    }
    
    return ret;
}

size_t ListIndex(const std::string& string, const std::string* strings, size_t defaultIndex)
{
    return ListIndex(string.c_str(), strings, defaultIndex);
}

size_t ListIndex(const char* string, const std::string* strings, size_t defaultIndex)
{
    size_t i = 0;

    while (!strings[i].empty())
    {
        if (strings[i] == string)
            return i;
        ++i;
    }

    return defaultIndex;
}

size_t ListIndex(const std::string& string, const char** strings, size_t defaultIndex)
{
    return ListIndex(string.c_str(), strings, defaultIndex);
}

size_t ListIndex(const char* string, const char** strings, size_t defaultIndex)
{
    size_t i = 0;

    while (strings[i])
    {
        if (!strcmp(string, strings[i]))
            return i;
        ++i;
    }

    return defaultIndex;
}

std::string FormatString(const char* formatString, ...)
{
    char formatBuffer[1024];
    va_list args;
    va_start(args, formatString);
    vsnprintf(formatBuffer, 1024, formatString, args);
    va_end(args);
    return std::string(formatBuffer);
}

std::string ToString(bool value)
{
    if (value)
        return "true";
    else
        return "false";
}

std::string ToString(short value)
{
    return FormatString("%d", value);
}

std::string ToString(int value)
{
    return FormatString("%d", value);
}

std::string ToString(long long value) 
{
    return FormatString("%lld", value);
}

std::string ToString(unsigned short value)
{
    return FormatString("%u", value);
}

std::string ToString(unsigned value) 
{
    return FormatString("%u", value);
}

std::string ToString(unsigned long long value)
{
    return FormatString("%llu", value);
}

std::string ToString(float value)
{
    return FormatString("%g", value);
}

std::string ToString(double value)
{
    return FormatString("%.15g", value);
}

int ParseInt(const std::string& string)
{
    return ParseInt(string.c_str());
}

int ParseInt(const char* string)
{
    char* ptr = const_cast<char*>(string);
    return strtol(ptr, &ptr, 10);
}

float ParseFloat(const std::string& string)
{
    return ParseFloat(string.c_str());
}

float ParseFloat(const char* string)
{
    char* ptr = const_cast<char*>(string);
    return (float)strtod(ptr, &ptr);
}
