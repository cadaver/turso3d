// For conditions of distribution and use, see copyright notice in License.txt

#include "Allocator.h"
#include "Vector.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

VectorBase::VectorBase() :
    buffer(0)
{
}

void VectorBase::Swap(VectorBase& vector)
{
    Turso3D::Swap(buffer, vector.buffer);
}

unsigned char* VectorBase::AllocateBuffer(size_t size)
{
    // Include space for size and capacity
    return new unsigned char[size + 2 * sizeof(size_t)];
}

template<> void Swap<VectorBase>(VectorBase& first, VectorBase& second)
{
    first.Swap(second);
}

}
