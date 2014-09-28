// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/WeakPtr.h"

namespace Turso3D
{

class Graphics;

/// Base class for objects that allocate GPU resources.
class TURSO3D_API GPUObject : public WeakRefCounted
{
public:
    /// Construct. Acquire the %Graphics subsystem if available and register self.
    GPUObject();
    /// Destruct. Unregister from the %Graphics subsystem.
    virtual ~GPUObject();
    
    /// Release the GPU resource.
    virtual void Release();

protected:
    /// %Graphics subsystem pointer.
    WeakPtr<Graphics> graphics;
};

}
