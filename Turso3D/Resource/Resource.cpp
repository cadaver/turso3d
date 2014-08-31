// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "Resource.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

bool Resource::BeginLoad(Stream&)
{
    return false;
}

bool Resource::EndLoad()
{
    // Resources that do not need access to main-thread critical objects do not need to override this
    return true;
}

bool Resource::Save(Stream&) const
{
    LOGERROR("Save not supported for " + TypeName());
    return false;
}

bool Resource::Load(Stream& source)
{
    bool success = BeginLoad(source);
    if (success)
        success &= EndLoad();

    return success;
}

void Resource::SetName(const String& newName)
{
    name = newName;
    nameHash = StringHash(newName);
}

}
