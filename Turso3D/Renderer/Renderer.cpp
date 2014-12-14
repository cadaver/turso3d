// For conditions of distribution and use, see copyright notice in License.txt

#include "../Scene/Scene.h"
#include "Camera.h"
#include "Renderer.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

void RegisterRendererLibrary()
{
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    // Scene node base attributes are needed
    RegisterSceneLibrary();
    Camera::RegisterObject();
}

}
