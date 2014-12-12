// For conditions of distribution and use, see copyright notice in License.txt

#include "GPUObject.h"
#include "Graphics.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

GPUObject::GPUObject() :
    dataLost(false)
{
    graphics = Object::Subsystem<Graphics>();
    if (graphics)
        graphics->AddGPUObject(this);
}

GPUObject::~GPUObject()
{
    if (graphics)
        graphics->RemoveGPUObject(this);
}

void GPUObject::Release()
{
}

void GPUObject::Recreate()
{
}

}

