// For conditions of distribution and use, see copyright notice in License.txt

#include "Ptr.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

RefCounted::RefCounted() :
    refCount(nullptr)
{
}

RefCounted::~RefCounted()
{
    if (refCount)
    {
        assert(refCount->refs == 0);
        if (refCount->weakRefs == 0)
            delete refCount;
        else
            refCount->expired = true;
    }
}

void RefCounted::AddRef()
{
    if (!refCount)
        refCount = new RefCount();

    ++(refCount->refs);
}

void RefCounted::ReleaseRef()
{
    assert(refCount && refCount->refs > 0);
    --(refCount->refs);
    if (refCount->refs == 0)
        delete this;
}

RefCount* RefCounted::RefCountPtr()
{
    if (!refCount)
        refCount = new RefCount();

    return refCount;
}

}
