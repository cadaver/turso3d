// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

namespace Turso3D
{

/// Swap two values.
template<class T> inline void Swap(T& first, T& second)
{
    T temp = first;
    first = second;
    second = temp;
}

}
