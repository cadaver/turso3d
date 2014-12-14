// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/Object.h"

namespace Turso3D
{

/// High-level rendering subsystem. Performs rendering of 3D scenes.
class Renderer : public Object
{
    /// Construct and register subsystem. 
    Renderer();
    /// Destruct.
    ~Renderer();
};

/// Register Renderer related object factories and attributes.
TURSO3D_API void RegisterRendererLibrary();

}

