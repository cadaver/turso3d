// For conditions of distribution and use, see copyright notice in License.txt

#include "Vector.h"
#include "DebugNew.h"

namespace Turso3D
{

unsigned char* VectorBase::AllocateBuffer(size_t size)
{
    return new unsigned char[size];
}

}
