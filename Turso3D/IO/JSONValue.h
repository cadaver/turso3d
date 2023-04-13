// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <map>
#include <string>
#include <vector>

class JSONValue;
class Stream;

typedef std::vector<JSONValue> JSONArray;
typedef std::map<std::string, JSONValue> JSONObject;

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
        char charValue;
        bool boolValue;
        double numberValue;
        void* objectValue;
    };
};

/// JSON value. Stores a boolean, string or number, or either an array or dictionary-like collection of nested values.
class JSONValue
{
    friend class JSONFile;
    
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
    JSONValue(const std::string& value);
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
    JSONValue& operator = (const std::string& value);
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
    JSONValue& operator [] (const std::string& key);
    /// Const index as an object. Return a null value if not an object.
    const JSONValue& operator [] (const std::string& key) const;
    /// Test for equality with another JSON value.
    bool operator == (const JSONValue& rhs) const;
    /// Test for inequality.
    bool operator != (const JSONValue& rhs) const { return !(*this == rhs); }
    
    /// Parse from a string. Return true on success.
    bool FromString(const std::string& str);
    /// Parse from a C string. Return true on success.
    bool FromString(const char* string);
    /// Parse from a binary stream.
    void FromBinary(Stream& source);
    /// Write to a string. Called recursively to write nested values.
    void ToString(std::string& dest, int spacing = 2, int indent = 0) const;
    /// Return as string.
    std::string ToString(int spacing = 2) const;
    /// Serialize to a binary stream.
    void ToBinary(Stream& dest) const;
    
    /// Push a value at the end. Becomes an array if was not before.
    void Push(const JSONValue& value);
    /// Insert a value at position. Becomes an array if was not before.
    void Insert(size_t index, const JSONValue& value);
    /// Remove the last value. No-op if not an array.
    void Pop();
    /// Remove indexed value. No-op if not an array.
    void Erase(size_t pos);
    /// Resize array. Becomes an array if was not before.
    void Resize(size_t newSize);
    /// Insert an associative value. Becomes an object if was not before.
    void Insert(const std::pair<std::string, JSONValue>& pair);
    /// Remove an associative value. No-op if not an object.
    void Erase(const std::string& key);
    /// Clear array or object. No-op otherwise.
    void Clear();
    /// Set to an empty array.
    void SetEmptyArray();
    /// Set to an empty object.
    void SetEmptyObject();
    /// Set to null value.
    void SetNull();
    
    /// Return number of values for objects or arrays, or 0 otherwise.
    size_t Size() const;
    /// Return whether an object or array is empty. Return false if not an object or array.
    bool Empty() const;
    
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
    bool GetBool() const { return type == JSON_BOOL ? data.boolValue : false; }
    /// Return value as a number, or zero on type mismatch.
    double GetNumber() const { return type == JSON_NUMBER ? data.numberValue : 0.0; }
    /// Return value as a string, or empty string on type mismatch.
    const std::string& GetString() const { return type == JSON_STRING ? *(reinterpret_cast<const std::string*>(data.objectValue)) : emptyString; }
    /// Return value as an array, or empty on type mismatch.
    const JSONArray& GetArray() const { return type == JSON_ARRAY ? *(reinterpret_cast<const JSONArray*>(data.objectValue)) : emptyJSONArray; }
    /// Return value as an object, or empty on type mismatch.
    const JSONObject& GetObject() const { return type == JSON_OBJECT ? *(reinterpret_cast<const JSONObject*>(data.objectValue)) : emptyJSONObject; }
    /// Return whether has an associative value.
    bool Contains(const std::string& key) const;
    
    /// Empty (null) value.
    static const JSONValue EMPTY;
    /// Empty string
    static const std::string emptyString;
    /// Empty array.
    static const JSONArray emptyJSONArray;
    /// Empty object.
    static const JSONObject emptyJSONObject;
    
private:
    /// Parse from a char buffer. Return true on success.
    bool Parse(const char*&pos, const char*& end);
    /// Assign a new type and perform the necessary dynamic allocation / deletion.
    void SetType(JSONType newType);
    
    /// Append a string in JSON format into the destination.
    static void WriteJSONString(std::string& dest, const std::string& str);
    /// Append indent spaces to the destination.
    static void WriteIndent(std::string& dest, int indent);
    /// Read a string in JSON format from a stream. Return true on success.
    static bool ReadJSONString(std::string& dest, const char*& pos, const char*& end, bool inQuote);
    /// Match until the end of a string. Return true if successfully matched.
    static bool MatchString(const char* str, const char*& pos, const char*& end);
    /// Scan until a character is found. Return true if successfully matched.
    static bool MatchChar(char c, const char*& pos, const char*& end);

    /// Return next char from buffer. Return true on success or false if the stream ended.
    static bool NextChar(char& dest, const char*& pos, const char*& end, bool skipWhiteSpace)
    {
        while (pos < end)
        {
            dest = *pos;
            ++pos;
            if (!skipWhiteSpace || dest > 0x20)
                return true;
        }
        
        return false;
    }
    
    /// Type.
    JSONType type;
    /// Value data.
    JSONData data;
};
