// For conditions of distribution and use, see copyright notice in License.txt

#include "Ptr.h"
#include "DebugNew.h"

namespace Turso3D
{

unsigned* WeakRefCounted::WeakRefCountPtr()
{
    if (!weakRefCount)
    {
        weakRefCount = new unsigned;
        *weakRefCount = 0;
    }
    
    return weakRefCount;
}

}
