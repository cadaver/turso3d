// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/HashMap.h"
#include "../Base/String.h"
#include "../Base/Vector.h"

namespace Turso3D
{

class Deserializer;
class JSONValue;
class Serializer;

typedef Vector<JSONValue> JSONArray;
typedef HashMap<String, JSONValue> JSONObject;

/// JSON value types.
enum JSONType
{
    JSON_NULL = 0,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
};

/// JSON data union.
struct JSONData
{
    union
    {
        bool boolValue;
        double numberValue;
        size_t padding[4];
    };
};

/// JSON value used in human-readable serialization.
class JSONValue
{
public:
    /// Construct a null value.
    JSONValue() :
        type(JSON_NULL)
    {
    }
    
    /// Copy-construct.
    JSONValue(const JSONValue& value) :
        type(JSON_NULL)
    {
        *this = value;
    }
    
    /// Construct from a boolean.
    JSONValue(bool value) :
        type(JSON_NULL)
    {
        *this = value;
    }
    
    /// Construct from an integer number.
    JSONValue(int value) :
        type(JSON_NULL)
    {
        *this = value;
    }
    /// Construct from an unsigned integer number.
    JSONValue(unsigned value) :
        type(JSON_NULL)
    {
        *this = value;
    }
    
    /// Construct from a floating point number.
    JSONValue(float value) :
        type(JSON_NULL)
    {
        *this = value;
    }
    
    /// Construct from a floating point number.
    JSONValue(double value) :
        type(JSON_NULL)
    {
        *this = value;
    }
    
    /// Construct from a string.
    JSONValue(const String& value) :
        type(JSON_NULL)
    {
        *this = value;
    }
    /// Construct from a C string.
    JSONValue(const char* value) :
        type(JSON_NULL)
    {
        *this = value;
    }
    
    /// Destruct.
    ~JSONValue()
    {
        SetType(JSON_NULL);
    }
    
    /// Assign a JSON value.
    JSONValue& operator = (const JSONValue& rhs);
    
    /// Assign a boolean.
    JSONValue& operator = (bool rhs)
    {
        SetType(JSON_BOOL);
        data.boolValue = rhs;
        return *this;
    }
    
    /// Assign an integer number.
    JSONValue& operator = (int rhs)
    {
        SetType(JSON_NUMBER);
        data.numberValue = (double)rhs;
        return *this;
    }
    
    /// Assign an unsigned integer number.
    JSONValue& operator = (unsigned rhs)
    {
        SetType(JSON_NUMBER);
        data.numberValue = (double)rhs;
        return *this;
    }
    
    /// Assign a floating point number.
    JSONValue& operator = (float rhs)
    {
        SetType(JSON_NUMBER);
        data.numberValue = (double)rhs;
        return *this;
    }
    
    /// Assign a floating point number.
    JSONValue& operator = (double rhs)
    {
        SetType(JSON_NUMBER);
        data.numberValue = rhs;
        return *this;
    }
    
    /// Assign a string.
    JSONValue& operator = (const String& value)
    {
        SetType(JSON_STRING);
        *(reinterpret_cast<String*>(&data)) = value;
        return *this;
    }
    
    /// Assign a C string.
    JSONValue& operator = (const char* value)
    {
        SetType(JSON_STRING);
        *(reinterpret_cast<String*>(&data)) = value;
        return *this;
    }
    
    /// Index as an array. Becomes an array if was not before.
    JSONValue& operator [] (size_t index)
    {
        if (type != JSON_ARRAY)
            SetType(JSON_ARRAY);
        
        return const_cast<JSONArray&>(AsArray())[index];
    }
    
    /// Const index as an array. Return a null value if not an array.
    const JSONValue& operator [] (size_t index) const
    {
        if (type == JSON_OBJECT)
        {
            const JSONArray& array = AsArray();
            return array[index];
        }
        else
            return EMPTY;
    }
    
    /// Index as an object. Becomes an object if was not before.
    JSONValue& operator [] (const String& key)
    {
        if (type != JSON_OBJECT)
            SetType(JSON_OBJECT);
        
        return const_cast<JSONObject&>(AsObject())[key];
    }
    
    /// Const index as an object. Return a null value if not an object.
    const JSONValue& operator [] (const String& key) const
    {
        if (type == JSON_OBJECT)
        {
            const JSONObject& object = AsObject();
            JSONObject::ConstIterator it = object.Find(key);
            return it != object.End() ? it->second : EMPTY;
        }
        else
            return EMPTY;
    }
    
    /// Test for equality with another JSON value.
    bool operator == (const JSONValue& rhs) const;
    /// Test for inequality.
    bool operator != (const JSONValue& rhs) const { return !(*this == rhs); }
    
    /// Push a value at the end. Becomes an array if was not before.
    void Push(const JSONValue& value)
    {
        SetType(JSON_ARRAY);
        const_cast<JSONArray&>(AsArray()).Push(value);
    }
    
    /// Insert a value at position. Becomes an array if was not before.
    void Insert(size_t index, const JSONValue& value)
    {
        SetType(JSON_ARRAY);
        const_cast<JSONArray&>(AsArray()).Insert(index, value);
    }
    
    /// Remove the last value. No-op if not an array.
    void Pop()
    {
        if (type == JSON_ARRAY)
            const_cast<JSONArray&>(AsArray()).Pop();
    }
    
    /// Remove indexed value(s). No-op if not an array.
    void Erase(size_t pos, size_t length = 1)
    {
        if (type == JSON_ARRAY)
            const_cast<JSONArray&>(AsArray()).Erase(pos, length);
    }
    
    /// Resize array. Becomes an array if was not before.
    void Resize(size_t newSize)
    {
        SetType(JSON_ARRAY);
        const_cast<JSONArray&>(AsArray()).Resize(newSize);
    }
    
    /// Insert an associative value. Becomes an object if was not before.
    void Insert(const Pair<String, JSONValue>& pair)
    {
        SetType(JSON_OBJECT);
        const_cast<JSONObject&>(AsObject()).Insert(pair);
    }
    
    /// Remove an associative value. No-op if not an object.
    void Erase(const String& key)
    {
        if (type == JSON_OBJECT)
            const_cast<JSONObject&>(AsObject()).Erase(key);
    }
    
    /// Clear array or object. No-op otherwise.
    void Clear()
    {
        if (type == JSON_ARRAY)
            const_cast<JSONArray&>(AsArray()).Clear();
        else if (type == JSON_OBJECT)
            const_cast<JSONObject&>(AsObject()).Clear();
    }
    
    /// Set to null value.
    void SetNull() { SetType(JSON_NULL); }
    
    /// Write to a stream. Return true on success.
    bool Write(Serializer& dest, int spacing = 2, int indent = 0) const;
    /// Serialize to a string.
    String ToString(int spacing = 2) const;
    
    /// Return value as a bool, or false on type mismatch.
    bool AsBool() const { return type == JSON_BOOL ? data.boolValue : false; }
    /// Return value as a number, or zero on type mismatch.
    double AsNumber() const { return type == JSON_NUMBER ? data.numberValue : 0.0; }
    /// Return value as a string, or empty string on type mismatch.
    const String& AsString() const { return type == JSON_STRING ? *(reinterpret_cast<const String*>(&data)) : String::EMPTY; }
    /// Return value as an array, or empty on type mismatch
    const JSONArray& AsArray() const { return type == JSON_ARRAY ? *(reinterpret_cast<const JSONArray*>(&data)) : emptyJSONArray; }
    /// Return value as an object, or empty on type mismatch
    const JSONObject& AsObject() const { return type == JSON_OBJECT ? *(reinterpret_cast<const JSONObject*>(&data)) : emptyJSONObject; }
    /// Return an associative value, or null if not an object.
    const JSONValue& Value(const String& key) const { return (*this)[key]; }
    /// Return an indexed value, or null if not an array.
    const JSONValue& At(size_t index) const { return (*this)[index]; }
    
    /// Return number of values for objects or arrays, or 0 otherwise.
    size_t Size() const
    {
        if (type == JSON_ARRAY)
            return AsArray().Size();
        else if (type == JSON_OBJECT)
            AsObject().Size();
        else
            return 0;
    }
    
    /// Return type.
    JSONType Type() const { return type; }
    /// Return whether is null.
    bool IsNull() const { return type == JSON_NULL; }
    /// Return whether is a bool.
    bool IsBool() const { return type == JSON_BOOL; }
    /// Return whether is a number.
    bool IsNumber() const { return type == JSON_NUMBER; }
    /// Return whether is a string.
    bool IsString() const { return type == JSON_STRING; }
    /// Return whether is an array.
    bool IsArray() const { return type == JSON_ARRAY; }
    /// Return whether is an object.
    bool IsObject() const { return type == JSON_OBJECT; }
    
    /// Empty (null) value.
    static const JSONValue EMPTY;
    /// Empty array.
    static const JSONArray emptyJSONArray;
    /// Empty object.
    static const JSONObject emptyJSONObject;
    
private:
    /// Assign a new type and perform the necessary dynamic allocation / deletion.
    void SetType(JSONType newType);
    
    /// Write a string in JSON format into a stream. Return true on success.
    static bool WriteJSONString(Serializer& dest, const String& str);

    /// Type.
    JSONType type;
    /// Value data.
    JSONData data;
};

}
