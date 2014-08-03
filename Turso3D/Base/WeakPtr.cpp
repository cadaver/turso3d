// For conditions of distribution and use, see copyright notice in License.txt

#include "WeakPtr.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

WeakRefCounted::WeakRefCounted() :
    refCount(0)
{
}

WeakRefCounted::~WeakRefCounted()
{
    if (refCount)
    {
        if (*refCount == 0)
            delete refCount;
        else
            *refCount |= EXPIRED;
    }
}

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
