// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

namespace Turso3D
{

class HashBase;
class ListBase;
class VectorBase;
class String;

/// Swap two values.
template<class T> inline void Swap(T& first, T& second)
{
    T temp = first;
    first = second;
    second = temp;
}

/// Swap two hash sets/maps.
template<> TURSO3D_API void Swap<HashBase>(HashBase& first, HashBase& second);

/// Swap two lists.
template<> TURSO3D_API void Swap<ListBase>(ListBase& first, ListBase& second);

/// Swap two vectors.
template<> TURSO3D_API void Swap<VectorBase>(VectorBase& first, VectorBase& second);

/// Swap two strings.
template<> TURSO3D_API void Swap<String>(String& first, String& second);

}
