// For conditions of distribution and use, see copyright notice in License.txt

#include "JSONValue.h"
#include "Stream.h"
#include "StringUtils.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

const JSONValue JSONValue::EMPTY;
const std::string JSONValue::emptyString;
const JSONArray JSONValue::emptyJSONArray;
const JSONObject JSONValue::emptyJSONObject;

JSONValue::JSONValue() :
    type(JSON_NULL)
{
}

JSONValue::JSONValue(const JSONValue& value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(bool value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(int value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(unsigned value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(float value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(double value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(const std::string& value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(const char* value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(const JSONArray& value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::JSONValue(const JSONObject& value) :
    type(JSON_NULL)
{
    *this = value;
}

JSONValue::~JSONValue()
{
    SetType(JSON_NULL);
}

JSONValue& JSONValue::operator = (const JSONValue& rhs)
{
    SetType(rhs.type);
    
    switch (type)
    {
    case JSON_BOOL:
        data.boolValue = rhs.data.boolValue;
        break;
        
    case JSON_NUMBER:
        data.numberValue = rhs.data.numberValue;
        break;
        
    case JSON_STRING:
        *(reinterpret_cast<std::string*>(data.objectValue)) = *(reinterpret_cast<const std::string*>(rhs.data.objectValue));
        break;
        
    case JSON_ARRAY:
        *(reinterpret_cast<JSONArray*>(data.objectValue)) = *(reinterpret_cast<const JSONArray*>(rhs.data.objectValue));
        break;
        
    case JSON_OBJECT:
        *(reinterpret_cast<JSONObject*>(data.objectValue)) = *(reinterpret_cast<const JSONObject*>(rhs.data.objectValue));
        break;
        
    default:
        break;
    }
    
    return *this;
}

JSONValue& JSONValue::operator = (bool rhs)
{
    SetType(JSON_BOOL);
    data.boolValue = rhs;
    return *this;
}

JSONValue& JSONValue::operator = (int rhs)
{
    SetType(JSON_NUMBER);
    data.numberValue = (double)rhs;
    return *this;
}

JSONValue& JSONValue::operator = (unsigned rhs)
{
    SetType(JSON_NUMBER);
    data.numberValue = (double)rhs;
    return *this;
}

JSONValue& JSONValue::operator = (float rhs)
{
    SetType(JSON_NUMBER);
    data.numberValue = (double)rhs;
    return *this;
}

JSONValue& JSONValue::operator = (double rhs)
{
    SetType(JSON_NUMBER);
    data.numberValue = rhs;
    return *this;
}

JSONValue& JSONValue::operator = (const std::string& value)
{
    SetType(JSON_STRING);
    *(reinterpret_cast<std::string*>(data.objectValue)) = value;
    return *this;
}

JSONValue& JSONValue::operator = (const char* value)
{
    SetType(JSON_STRING);
    *(reinterpret_cast<std::string*>(data.objectValue)) = value;
    return *this;
}

JSONValue& JSONValue::operator = (const JSONArray& value)
{
    SetType(JSON_ARRAY);
    *(reinterpret_cast<JSONArray*>(data.objectValue)) = value;
    return *this;
}

JSONValue& JSONValue::operator = (const JSONObject& value)
{
    SetType(JSON_OBJECT);
    *(reinterpret_cast<JSONObject*>(data.objectValue)) = value;
    return *this;
}

JSONValue& JSONValue::operator [] (size_t index)
{
    if (type != JSON_ARRAY)
        SetType(JSON_ARRAY);
    
    return (*(reinterpret_cast<JSONArray*>(data.objectValue)))[index];
}

const JSONValue& JSONValue::operator [] (size_t index) const
{
    if (type == JSON_ARRAY)
        return (*(reinterpret_cast<const JSONArray*>(data.objectValue)))[index];
    else
        return EMPTY;
}

JSONValue& JSONValue::operator [] (const std::string& key)
{
    if (type != JSON_OBJECT)
        SetType(JSON_OBJECT);
    
    return (*(reinterpret_cast<JSONObject*>(data.objectValue)))[key];
}

const JSONValue& JSONValue::operator [] (const std::string& key) const
{
    if (type == JSON_OBJECT)
    {
        const JSONObject& object = *(reinterpret_cast<const JSONObject*>(data.objectValue));
        auto it = object.find(key);
        return it != object.end() ? it->second : EMPTY;
    }
    else
        return EMPTY;
}

bool JSONValue::operator == (const JSONValue& rhs) const
{
    if (type != rhs.type)
        return false;
    
    switch (type)
    {
    case JSON_BOOL:
        return data.boolValue == rhs.data.boolValue;

    case JSON_NUMBER:
        return data.numberValue == rhs.data.numberValue;

    case JSON_STRING:
        return *(reinterpret_cast<const std::string*>(data.objectValue)) == *(reinterpret_cast<const std::string*>(rhs.data.objectValue));

    case JSON_ARRAY:
        return *(reinterpret_cast<const JSONArray*>(data.objectValue)) == *(reinterpret_cast<const JSONArray*>(rhs.data.objectValue));

    case JSON_OBJECT:
        return *(reinterpret_cast<const JSONObject*>(data.objectValue)) == *(reinterpret_cast<const JSONObject*>(rhs.data.objectValue));

    default:
        return true;
    }
}

bool JSONValue::FromString(const std::string& str)
{
    const char* pos = str.c_str();
    const char* end = pos + str.length();
    return Parse(pos, end);
}

bool JSONValue::FromString(const char* string)
{
    if (!string)
        return false;

    const char* pos = string;
    const char* end = pos + strlen(string);
    return Parse(pos, end);
}

void JSONValue::FromBinary(Stream& source)
{
    JSONType newType = (JSONType)source.Read<unsigned char>();

    switch (newType)
    {
    case JSON_NULL:
        Clear();
        break;

    case JSON_BOOL:
        *this = source.Read<bool>();
        break;

    case JSON_NUMBER:
        *this = source.Read<double>();
        break;

    case JSON_STRING:
        *this = source.Read<std::string>();
        break;

    case JSON_ARRAY:
        {
            SetEmptyArray();
            size_t num = source.ReadVLE();
            for (size_t i = 0; i < num && !source.IsEof(); ++i)
                Push(source.Read<JSONValue>());
        }
        break;

    case JSON_OBJECT:
        {
            SetEmptyObject();
            size_t num = source.ReadVLE();
            for (size_t i = 0; i < num && !source.IsEof(); ++i)
            {
                std::string key = source.Read<std::string>();
                (*this)[key] = source.Read<JSONValue>();
            }
        }
        break;

    default:
        break;
    }
}

void JSONValue::ToString(std::string& dest, int spacing, int indent) const
{
    switch (type)
    {
    case JSON_BOOL:
        dest += ::ToString(data.boolValue);
        return;
        
    case JSON_NUMBER:
        dest += ::ToString(data.numberValue);
        return;
        
    case JSON_STRING:
        WriteJSONString(dest, *(reinterpret_cast<const std::string*>(data.objectValue)));
        return;
        
    case JSON_ARRAY:
        {
            const JSONArray& array = GetArray();
            dest += '[';
            
            if (array.size())
            {
                indent += spacing;
                for (auto it = array.begin(); it < array.end(); ++it)
                {
                    if (it != array.begin())
                        dest += ',';
                    dest += '\n';
                    WriteIndent(dest, indent);
                    it->ToString(dest, spacing, indent);
                }
                indent -= spacing;
                dest += '\n';
                WriteIndent(dest, indent);
            }
            
            dest += ']';
        }
        break;
        
    case JSON_OBJECT:
        {
            const JSONObject& object = GetObject();
            dest += '{';
            
            if (object.size())
            {
                indent += spacing;
                for (auto it = object.begin(); it != object.end(); ++it)
                {
                    if (it != object.begin())
                        dest += ',';
                    dest += '\n';
                    WriteIndent(dest, indent);
                    WriteJSONString(dest, it->first);
                    dest += ": ";
                    it->second.ToString(dest, spacing, indent);
                }
                indent -= spacing;
                dest += '\n';
                WriteIndent(dest, indent);
            }
            
            dest += '}';
        }
        break;
        
    default:
        dest += "null";
    }
}

std::string JSONValue::ToString(int spacing) const
{
    std::string ret;
    ToString(ret, spacing);
    return ret;
}

void JSONValue::ToBinary(Stream& dest) const
{
    dest.Write((unsigned char)type);

    switch (type)
    {
    case JSON_BOOL:
        dest.Write(data.boolValue);
        break;

    case JSON_NUMBER:
        dest.Write(data.numberValue);
        break;

    case JSON_STRING:
        dest.Write(GetString());
        break;

    case JSON_ARRAY:
        {
            const JSONArray& array = GetArray();
            dest.WriteVLE(array.size());
            for (auto it = array.begin(); it != array.end(); ++it)
                it->ToBinary(dest);
        }
        break;

    case JSON_OBJECT:
        {
            const JSONObject& object = GetObject();
            dest.WriteVLE(object.size());
            for (auto it = object.begin(); it != object.end(); ++it)
            {
                dest.Write(it->first);
                it->second.ToBinary(dest);
            }
        }
        break;

    default:
        break;
    }
}

void JSONValue::Push(const JSONValue& value)
{
    SetType(JSON_ARRAY);
    (*(reinterpret_cast<JSONArray*>(data.objectValue))).push_back(value);
}

void JSONValue::Insert(size_t index, const JSONValue& value)
{
    SetType(JSON_ARRAY);
    JSONArray& arr = (*(reinterpret_cast<JSONArray*>(data.objectValue)));
    arr.insert(arr.begin() + index, value);
}

void JSONValue::Pop()
{
    if (type == JSON_ARRAY)
        (*(reinterpret_cast<JSONArray*>(data.objectValue))).pop_back();
}

void JSONValue::Erase(size_t pos)
{
    if (type == JSON_ARRAY)
    {
        JSONArray& arr = (*(reinterpret_cast<JSONArray*>(data.objectValue)));
        (*(reinterpret_cast<JSONArray*>(data.objectValue))).erase(arr.begin() + pos);
    }
}

void JSONValue::Resize(size_t newSize)
{
    SetType(JSON_ARRAY);
    (*(reinterpret_cast<JSONArray*>(data.objectValue))).resize(newSize);
}

void JSONValue::Insert(const std::pair<std::string, JSONValue>& pair)
{
    SetType(JSON_OBJECT);
    JSONObject& obj = (*(reinterpret_cast<JSONObject*>(data.objectValue)));
    obj[pair.first] = pair.second;
}

void JSONValue::Erase(const std::string& key)
{
    if (type == JSON_OBJECT)
        (*(reinterpret_cast<JSONObject*>(data.objectValue))).erase(key);
}

void JSONValue::Clear()
{
    if (type == JSON_ARRAY)
        (*(reinterpret_cast<JSONArray*>(data.objectValue))).clear();
    else if (type == JSON_OBJECT)
        (*(reinterpret_cast<JSONObject*>(data.objectValue))).clear();
}

void JSONValue::SetEmptyArray()
{
    SetType(JSON_ARRAY);
    Clear();
}

void JSONValue::SetEmptyObject()
{
    SetType(JSON_OBJECT);
    Clear();
}

void JSONValue::SetNull()
{
    SetType(JSON_NULL);
}

size_t JSONValue::Size() const
{
    if (type == JSON_ARRAY)
        return (*(reinterpret_cast<const JSONArray*>(data.objectValue))).size();
    else if (type == JSON_OBJECT)
        return (*(reinterpret_cast<const JSONObject*>(data.objectValue))).size();
    else
        return 0;
}

bool JSONValue::Empty() const
{
    if (type == JSON_ARRAY)
        return (*(reinterpret_cast<const JSONArray*>(data.objectValue))).empty();
    else if (type == JSON_OBJECT)
        return (*(reinterpret_cast<const JSONObject*>(data.objectValue))).empty();
    else
        return false;
}

bool JSONValue::Contains(const std::string& key) const
{
    if (type == JSON_OBJECT)
    {
        JSONObject& obj = (*(reinterpret_cast<JSONObject*>(data.objectValue)));
        return obj.find(key) != obj.end();
    }
    else
        return false;
}

bool JSONValue::Parse(const char*& pos, const char*& end)
{
    char c;

    // Handle comments
    for (;;)
    {
        if (!NextChar(c, pos, end, true))
            return false;

        if (c == '/')
        {
            if (!NextChar(c, pos, end, false))
                return false;
            if (c == '/')
            {
                // Skip until end of line
                if (!MatchChar('\n', pos, end))
                    return false;
            }
            else if (c == '*')
            {
                // Skip until end of comment
                if (!MatchChar('*', pos, end))
                    return false;
                if (!MatchChar('/', pos, end))
                    return false;
            }
            else
                return false;
        }
        else
            break;
    }

    if (c == '}' || c == ']')
        return false;
    else if (c == 'n')
    {
        SetNull();
        return MatchString("ull", pos, end);
    }
    else if (c == 'f')
    {
        *this = false;
        return MatchString("alse", pos, end);
    }
    else if (c == 't')
    {
        *this = true;
        return MatchString("rue", pos, end);
    }
    else if (isdigit(c) || c == '-')
    {
        --pos;
        *this = strtod(pos, const_cast<char**>(&pos));
        return true;
    }
    else if (c == '\"')
    {
        SetType(JSON_STRING);
        return ReadJSONString(*(reinterpret_cast<std::string*>(data.objectValue)), pos, end, true);
    }
    else if (c == '[')
    {
        SetEmptyArray();
        // Check for empty first
        if (!NextChar(c, pos, end, true))
            return false;
        if (c == ']')
            return true;
        else
            --pos;
        
        for (;;)
        {
            JSONValue arrayValue;
            if (!arrayValue.Parse(pos, end))
                return false;
            Push(arrayValue);
            if (!NextChar(c, pos, end, true))
                return false;
            if (c == ']')
                break;
            else if (c != ',')
                return false;
        }
        return true;
    }
    else if (c == '{')
    {
        SetEmptyObject();
        if (!NextChar(c, pos, end, true))
            return false;
        if (c == '}')
            return true;
        else
            --pos;
        
        for (;;)
        {
            std::string key;
            if (!ReadJSONString(key, pos, end, false))
                return false;
            if (!NextChar(c, pos, end, true))
                return false;
            if (c != ':')
                return false;
            
            JSONValue objectValue;
            if (!objectValue.Parse(pos, end))
                return false;
            (*this)[key] = objectValue;
            if (!NextChar(c, pos, end, true))
                return false;
            if (c == '}')
                break;
            else if (c != ',')
                return false;
        }
        return true;
    }
    
    return false;
}

void JSONValue::SetType(JSONType newType)
{
    if (type == newType)
        return;
    
    switch (type)
    {
    case JSON_STRING:
        delete reinterpret_cast<std::string*>(data.objectValue);
        break;
        
    case JSON_ARRAY:
        delete reinterpret_cast<JSONArray*>(data.objectValue);
        break;
        
    case JSON_OBJECT:
        delete reinterpret_cast<JSONObject*>(data.objectValue);
        break;
        
    default:
        break;
    }
    
    type = newType;
    
    switch (type)
    {
    case JSON_STRING:
        data.objectValue = new std::string();
        break;
        
    case JSON_ARRAY:
        data.objectValue = new JSONArray();
        break;
        
    case JSON_OBJECT:
        data.objectValue = new JSONObject();
        break;
        
    default:
        break;
    }
}

void JSONValue::WriteJSONString(std::string& dest, const std::string& str)
{
    dest += '\"';
    
    for (auto it = str.begin(); it != str.end(); ++it)
    {
        char c = *it;
        
        if (c >= 0x20 && c != '\"' && c != '\\')
            dest += c;
        else
        {
            dest += '\\';
            
            switch (c)
            {
            case '\"':
            case '\\':
                dest += c;
                break;
                
            case '\b':
                dest += 'b';
                break;
                
            case '\f':
                dest += 'f';
                break;
                
            case '\n':
                dest += 'n';
                break;
                
            case '\r':
                dest += 'r';
                break;
                
            case '\t':
                dest += 't';
                break;
                
            default:
                {
                    char buffer[6];
                    sprintf(buffer, "u%04x", c);
                    dest += buffer;
                }
                break;
            }
        }
    }
    
    dest += '\"';
}

void JSONValue::WriteIndent(std::string& dest, int indent)
{
    size_t oldLength = dest.length();
    dest.resize(oldLength + indent);
    for (int i = 0; i < indent; ++i)
        dest[oldLength + i] = ' ';
}

bool JSONValue::ReadJSONString(std::string& dest, const char*& pos, const char*& end, bool inQuote)
{
    char c;
    
    if (!inQuote)
    {
        if (!NextChar(c, pos, end, true) || c != '\"')
            return false;
    }
    
    dest.clear();
    for (;;)
    {
        if (!NextChar(c, pos, end, false))
            return false;
        if (c == '\"')
            break;
        else if (c != '\\')
            dest += c;
        else
        {
            if (!NextChar(c, pos, end, false))
                return false;
            switch (c)
            {
            case '\\':
                dest += '\\';
                break;
                
            case '\"':
                dest += '\"';
                break;
                
            case 'b':
                dest += '\b';
                break;
                
            case 'f':
                dest += '\f';
                break;
                
            case 'n':
                dest += '\n';
                break;
            
            case 'r':
                dest += '\r';
                break;
                
            case 't':
                dest += '\t';
                break;
                
            case 'u':
                {
                    /// \todo Doesn't handle unicode
                    unsigned code;
                    sscanf(pos, "%x", &code);
                    pos += 4;
                    dest += (char)code;
                }
                break;
            }
        }
    }
    
    return true;
}

bool JSONValue::MatchString(const char* str, const char*& pos, const char*& end)
{
    while (*str)
    {
        if (pos >= end || *pos != *str)
            return false;
        else
        {
            ++pos;
            ++str;
        }
    }
    
    return true;
}

bool JSONValue::MatchChar(char c, const char*& pos, const char*& end)
{
    char next;

    while (NextChar(next, pos, end, false))
    {
        if (next == c)
            return true;
    }
    return false;
}
