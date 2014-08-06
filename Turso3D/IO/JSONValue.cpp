// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/Vector.h"
#include "../Base/HashMap.h"
#include "JSONValue.h"

namespace Turso3D
{

const JSONValue JSONValue::EMPTY;
const JSONArray JSONValue::emptyJSONArray;
const JSONObject JSONValue::emptyJSONObject;

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
        break;
        
    case JSON_ARRAY:
        return *(reinterpret_cast<const JSONArray*>(&data)) == *(reinterpret_cast<const JSONArray*>(&rhs.data));
        break;
        
    case JSON_OBJECT:
        return *(reinterpret_cast<const JSONObject*>(&data)) == *(reinterpret_cast<const JSONObject*>(&rhs.data));
        break;
        
    default:
        return true;
    }
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

}
