// For conditions of distribution and use, see copyright notice in License.txt

#include "Allocator.h"
#include "Ptr.h"

static Allocator<RefCount> refCountAllocator;

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
            refCountAllocator.Free(refCount);
        else
            refCount->expired = true;
    }
}

void RefCounted::AddRef()
{
    if (!refCount)
        refCount = refCountAllocator.Allocate();

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
        refCount = refCountAllocator.Allocate();

    return refCount;
}

RefCount* RefCounted::AllocateRefCount()
{
    return refCountAllocator.Allocate();
}

void RefCounted::FreeRefCount(RefCount* refCount)
{
    refCountAllocator.Free(refCount);
}
