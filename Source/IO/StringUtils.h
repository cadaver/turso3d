// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <string>
#include <vector>

/// Count number of elements in a string.
size_t CountElements(const std::string& string, char separator = ' ');
/// Count number of elements in a string.
size_t CountElements(const char* string, char separator = ' ');
/// Trim whitespace from a string.
std::string Trim(const std::string& string);
/// Replace all occurrences of a substring in a string.
std::string Replace(const std::string& string, const std::string& replaceThis, const std::string& replaceWith);
/// Replace all occurrences of a char in a string.
std::string Replace(const std::string& string, char replaceThis, char replaceWith);
/// Replace all occurrences of a substring in a string.
void ReplaceInPlace(std::string& string, const std::string& replaceThis, const std::string& replaceWith);
/// Replace all occurrences of a char in a string.
void ReplaceInPlace(std::string& string, char replaceThis, char replaceWith);
/// Convert string to uppercase.
std::string ToUpper(const std::string& string);
/// Convert string to lowercase.
std::string ToLower(const std::string& string);
/// Check if string starts with another string.
bool StartsWith(const std::string& string, const std::string& substring);
/// Check if string ends with another string.
bool EndsWith(const std::string& string, const std::string& substring);
/// Split a string with separator.
std::vector<std::string> Split(const std::string& string, char separator = ' ');
/// Split a string with separator.
std::vector<std::string> Split(const char* string, char separator = ' ');
/// Return an index to a string list corresponding to the given string, or a default value if not found. The string list must be empty-terminated.
size_t ListIndex(const std::string& string, const std::string* strings, size_t defaultIndex);
/// Return an index to a string list corresponding to the given C string, or a default value if not found. The string list must be empty-terminated.
size_t ListIndex(const char* string, const std::string* strings, size_t defaultIndex);
/// Return an index to a C string list corresponding to the given string, or a default value if not found. The string list must be null-terminated.
size_t ListIndex(const std::string& string, const char** strings, size_t defaultIndex);
/// Return an index to a C string list corresponding to the given C string, or a default value if not found. The string list must be null-terminated.
size_t ListIndex(const char* string, const char** strings, size_t defaultIndex);
/// Return a formatted string.
std::string FormatString(const char* formatString, ...);
/// Convert value to string.
std::string ToString(bool value);
/// Convert value to string.
std::string ToString(short value);
/// Convert value to string.
std::string ToString(int value);
/// Convert value to string.
std::string ToString(long long value);
/// Convert value to string.
std::string ToString(unsigned short value);
/// Convert value to string.
std::string ToString(unsigned value);
/// Convert value to string.
std::string ToString(unsigned long long value);
/// Convert value to string.
std::string ToString(float value);
/// Convert value to string.
std::string ToString(double value);
/// Parse an integer value from a string.
int ParseInt(const std::string& string);
/// Parse an integer value from a string.
int ParseInt(const char* string);
/// Parse a floating-point value from a string.
float ParseFloat(const std::string& string);
/// Parse a floating-point value from a string.
float ParseFloat(const char* string);

