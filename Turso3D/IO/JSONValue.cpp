// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/Vector.h"
#include "../Base/HashMap.h"
#include "JSONValue.h"
#include "Serializer.h"
#include "VectorBuffer.h"

#include <cstdio>

namespace Turso3D
{

const JSONValue JSONValue::EMPTY;
const JSONArray JSONValue::emptyJSONArray;
const JSONObject JSONValue::emptyJSONObject;

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
        *(reinterpret_cast<String*>(&data)) = *(reinterpret_cast<const String*>(&rhs.data));
        break;
        
    case JSON_ARRAY:
        *(reinterpret_cast<JSONArray*>(&data)) = *(reinterpret_cast<const JSONArray*>(&rhs.data));
        break;
        
    case JSON_OBJECT:
        *(reinterpret_cast<JSONObject*>(&data)) = *(reinterpret_cast<const JSONObject*>(&rhs.data));
        break;
        
    default:
        break;
    }
    
    return *this;
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
        return *(reinterpret_cast<const String*>(&data)) == *(reinterpret_cast<const String*>(&rhs.data));
        
    case JSON_ARRAY:
        return *(reinterpret_cast<const JSONArray*>(&data)) == *(reinterpret_cast<const JSONArray*>(&rhs.data));
        
    case JSON_OBJECT:
        return *(reinterpret_cast<const JSONObject*>(&data)) == *(reinterpret_cast<const JSONObject*>(&rhs.data));
        
    default:
        return true;
    }
}

bool JSONValue::Write(Serializer& dest, int spacing, int indent) const
{
    switch (type)
    {
    case JSON_BOOL:
        return dest.WriteString(data.boolValue ? "true" : "false", false);
        
    case JSON_NUMBER:
        return dest.WriteString(String(data.numberValue), false);
        
    case JSON_STRING:
        return WriteJSONString(dest, *(reinterpret_cast<const String*>(&data)));
        
    case JSON_ARRAY:
        {
            const JSONArray& array = AsArray();
            if (!dest.WriteByte('['))
                return false;
            
            if (array.Size())
            {
                indent += spacing;
                for (size_t i = 0; i < array.Size(); ++i)
                {
                    if (i > 0)
                    {
                        if (!dest.WriteByte(','))
                            return false;
                    }
                    if (!dest.WriteByte('\n'))
                        return false;
                    if (!dest.WriteString(String(' ', indent), false))
                        return false;
                    if (!array[i].Write(dest, indent, spacing))
                        return false;
                }
                indent -= spacing;
                if (!dest.WriteByte('\n'))
                    return false;
                if (!dest.WriteString(String(' ', indent), false))
                    return false;
            }
            if (!dest.WriteByte(']'))
                return false;
        }
        break;
        
    case JSON_OBJECT:
        {
            const JSONObject& object = AsObject();
            if (!dest.WriteByte('{'))
                return false;
            
            if (object.Size())
            {
                indent += spacing;
                for (JSONObject::ConstIterator i = object.Begin(); i != object.End(); ++i)
                {
                    if (i != object.Begin())
                    {
                        if (!dest.WriteByte(','))
                            return false;
                    }
                    if (!dest.WriteByte('\n'))
                        return false;
                    if (!dest.WriteString(String(' ', indent), false))
                        return false;
                    if (!WriteJSONString(dest, i->first))
                        return false;
                    if (!dest.WriteString(": ", false))
                        return false;
                    if (!i->second.Write(dest, indent, spacing))
                        return false;
                }
                indent -= spacing;
                if (!dest.WriteByte('\n'))
                    return false;
                if (!dest.WriteString(String(' ', indent), false))
                    return false;
            }
            if (!dest.WriteByte('}'))
                return false;
        }
        break;
        
    default:
        return dest.WriteString("null", false);
    }
    
    return true;
}

String JSONValue::ToString(int spacing) const
{
    VectorBuffer dest;
    Write(dest, spacing);
    return String((const char*)dest.Data(), dest.Size());
}

void JSONValue::SetType(JSONType newType)
{
    if (type == newType)
        return;
    
    switch (type)
    {
    case JSON_STRING:
        (reinterpret_cast<String*>(&data))->~String();
        break;
        
    case JSON_ARRAY:
        (reinterpret_cast<JSONArray*>(&data))->~JSONArray();
        break;
        
    case JSON_OBJECT:
        (reinterpret_cast<JSONObject*>(&data))->~JSONObject();
        break;
    }
    
    type = newType;
    
    switch (type)
    {
    case JSON_STRING:
        new(reinterpret_cast<String*>(&data)) String();
        break;
        
    case JSON_ARRAY:
        new(reinterpret_cast<JSONArray*>(&data)) JSONArray();
        break;
        
    case JSON_OBJECT:
        new(reinterpret_cast<JSONObject*>(&data)) JSONObject();
        break;
    }
}

bool JSONValue::WriteJSONString(Serializer& dest, const String& str)
{
    if (!dest.WriteByte('\"'))
        return false;
        
    for (size_t i = 0; i < str.Length(); ++i)
    {
        char c = str[i];
        if (c < 0x20 || c == '\"' || c == '\\')
        {
            if (!dest.WriteByte('\\'))
                return false;
                
            switch (c)
            {
            case '\"':
                if (!dest.WriteByte('\"'))
                    return false;
                break;
                
            case '\\':
                if (!dest.WriteByte('\\'))
                    return false;
                break;
                
            case '\b':
                if (!dest.WriteByte('b'))
                    return false;
                break;
                
            case '\f':
                if (!dest.WriteByte('f'))
                    return false;
                break;
                
            case '\n':
                if (!dest.WriteByte('n'))
                    return false;
                break;
                
            case '\r':
                if (!dest.WriteByte('r'))
                    return false;
                break;
                
            case '\t':
                if (!dest.WriteByte('t'))
                    return false;
                break;
                
            default:
                {
                    char buffer[6];
                    sprintf(buffer, "u%04x", c);
                    if (!dest.WriteString(buffer, false))
                        return false;
                }
                break;
            }
        }
        else
            if (!dest.WriteByte(c))
                return false;
    }
    
    return dest.WriteByte('\"');
}

}
