// For conditions of distribution and use, see copyright notice in License.txt

#include "SharedPtr.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

RefCounted::RefCounted() :
    refCount(0)
{
}

RefCounted::~RefCounted()
{
    assert(refCount == 0);
}

void RefCounted::AddRef()
{
    ++refCount;
}

void RefCounted::ReleaseRef()
{
    assert(refCount > 0);
    --refCount;
    if (!refCount)
        delete this;
}

}
