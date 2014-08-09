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
    JSON_OBJECT,
    MAX_JSON_TYPES
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
class TURSO3D_API JSONValue
{
public:
    /// Construct a null value.
    JSONValue();
    /// Copy-construct.
    JSONValue(const JSONValue& value);
    /// Construct from a boolean.
    JSONValue(bool value);
    /// Construct from an integer number.
    JSONValue(int value);
    /// Construct from an unsigned integer number.
    JSONValue(unsigned value);
    /// Construct from a floating point number.
    JSONValue(float value);
    /// Construct from a floating point number.
    JSONValue(double value);
    /// Construct from a string.
    JSONValue(const String& value);
    /// Construct from a C string.
    JSONValue(const char* value);
    /// Construct from a JSON object.
    JSONValue(const JSONArray& value);
    /// Construct from a JSON object.
    JSONValue(const JSONObject& value);
    /// Destruct.
    ~JSONValue();
    
    /// Assign a JSON value.
    JSONValue& operator = (const JSONValue& rhs);
    /// Assign a boolean.
    JSONValue& operator = (bool rhs);
    /// Assign an integer number.
    JSONValue& operator = (int rhs);
    /// Assign an unsigned integer number.
    JSONValue& operator = (unsigned rhs);
    /// Assign a floating point number.
    JSONValue& operator = (float rhs);
    /// Assign a floating point number.
    JSONValue& operator = (double rhs);
    /// Assign a string.
    JSONValue& operator = (const String& value);
    /// Assign a C string.
    JSONValue& operator = (const char* value);
    /// Assign a JSON array.
    JSONValue& operator = (const JSONArray& value);
    /// Assign a JSON object.
    JSONValue& operator = (const JSONObject& value);
    /// Index as an array. Becomes an array if was not before.
    JSONValue& operator [] (size_t index);
    /// Const index as an array. Return a null value if not an array.
    const JSONValue& operator [] (size_t index) const;
    /// Index as an object. Becomes an object if was not before.
    JSONValue& operator [] (const String& key);
    /// Const index as an object. Return a null value if not an object.
    const JSONValue& operator [] (const String& key) const;
    /// Test for equality with another JSON value.
    bool operator == (const JSONValue& rhs) const;
    /// Test for inequality.
    bool operator != (const JSONValue& rhs) const { return !(*this == rhs); }
    
    /// Read from a stream. Return true on success.
    bool Read(Deserializer& source);
    /// Set from a string. Return true on success.
    bool FromString(const String& str);
    /// Set from a C string. Return true on success.
    bool FromString(const char* str);
    /// Push a value at the end. Becomes an array if was not before.
    void Push(const JSONValue& value);
    /// Insert a value at position. Becomes an array if was not before.
    void Insert(size_t index, const JSONValue& value);
    /// Remove the last value. No-op if not an array.
    void Pop();
    /// Remove indexed value(s). No-op if not an array.
    void Erase(size_t pos, size_t length = 1);
    /// Resize array. Becomes an array if was not before.
    void Resize(size_t newSize);
    /// Insert an associative value. Becomes an object if was not before.
    void Insert(const Pair<String, JSONValue>& pair);
    /// Remove an associative value. No-op if not an object.
    void Erase(const String& key);
    /// Clear array or object. No-op otherwise.
    void Clear();
    /// Set to null value.
    void SetNull();
    
    /// Write to a stream. Return true on success.
    bool Write(Serializer& dest, int spacing = 2, int indent = 0) const;
    /// Serialize to a string.
    String ToString(int spacing = 2) const;
    /// Return number of values for objects or arrays, or 0 otherwise.
    size_t Size() const;
    /// Return whether an object or array is empty. Return false if not an object or array.
    bool IsEmpty() const;
    
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
    /// Read a string in JSON format from a stream. Return true on success.
    static bool ReadJSONString(String& dest, Deserializer& source, bool inQuote);
    /// Get the next char from a stream. Return true on success or false if the stream ended.
    static bool NextChar(char& dest, Deserializer& source, bool skipWhiteSpace);
    /// Match until the end of a string. Return true if successfully matched.
    static bool MatchString(Deserializer& source, const char* str);
    
    /// Type.
    JSONType type;
    /// Value data.
    JSONData data;
};

}
