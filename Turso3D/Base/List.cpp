// For conditions of distribution and use, see copyright notice in License.txt

#include "List.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

template<> void Swap<ListBase>(ListBase& first, ListBase& second)
{
    first.Swap(second);
}

void ListBase::AllocatePtrs()
{
    ptrs = new ListNodeBase*[3];

    SetSize(0);
    ptrs[1] = nullptr;
    ptrs[2] = nullptr;
}

}
