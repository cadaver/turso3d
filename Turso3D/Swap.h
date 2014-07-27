// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

namespace Turso3D
{

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

template<> void Swap<ListBase>(ListBase& first, ListBase& second);
template<> void Swap<VectorBase>(VectorBase& first, VectorBase& second);
template<> void Swap<String>(String& first, String& second);

}
