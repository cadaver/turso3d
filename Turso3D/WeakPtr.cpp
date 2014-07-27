// For conditions of distribution and use, see copyright notice in License.txt

#include "WeakPtr.h"
#include "DebugNew.h"

namespace Turso3D
{

unsigned* WeakRefCounted::WeakRefCountPtr()
{
    if (!refCount)
    {
        refCount = new unsigned;
        *refCount = 0;
    }
    
    return refCount;
}

}
