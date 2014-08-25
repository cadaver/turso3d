// For conditions of distribution and use, see copyright notice in License.txt

#include "Turso3D.h"
#include "Debug/DebugNew.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include <cstdio>
#include <cstdlib>

using namespace Turso3D;

int main()
{
    #ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    
    RegisterResourceLibrary();
    
    Log log;
    Profiler profiler;
    ResourceCache cache;
    
    printf("Testing resource loading\n");
    
    Image* image = 0;

    {
        profiler.BeginFrame();
        cache.AddResourceDir(ExecutableDir() + "Data");
        image = cache.LoadResource<Image>("Test.png");
        profiler.EndFrame();
    }

    if (image)
        printf("Image loaded successfully, size %dx%d components %d\n", image->Width(), image->Height(), image->Components());

    LOGRAW(profiler.OutputResults(false, false, 16));

    return 0;
}