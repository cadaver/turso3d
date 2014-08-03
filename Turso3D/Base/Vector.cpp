// For conditions of distribution and use, see copyright notice in License.txt

#include "Allocator.h"
#include "Vector.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

void VectorBase::Swap(VectorBase& vector)
{
    Turso3D::Swap(size, vector.size);
    Turso3D::Swap(capacity, vector.capacity);
    Turso3D::Swap(buffer, vector.buffer);
}

unsigned char* VectorBase::AllocateBuffer(size_t size)
{
    return new unsigned char[size];
}

template<> void Swap<VectorBase>(VectorBase& first, VectorBase& second)
{
    first.Swap(second);
}

}
