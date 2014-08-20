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
    
    printf("Size of Node: %d\n", sizeof(Node));
    printf("Size of Scene: %d\n\n", sizeof(Scene));

    RegisterSceneLibrary();

    Log log;
    log.Open("03_Scene.log");
    
    Scene scene;
    for (size_t i = 0; i < 10; ++i)
        scene.CreateChild<Node>("Child" + String(i));

    {
        File binaryFile("Scene.bin", FILE_WRITE);
        scene.Save(binaryFile);
    }

    {
        File jsonFile("Scene.json", FILE_WRITE);
        scene.SaveJSON(jsonFile);
    }

    {
        File loadFile("Scene.bin", FILE_READ);
        Scene loadScene;
        if (loadScene.Load(loadFile))
        {
            printf("Scene loaded successfully from binary data\n");
            for (size_t i = 0; i < loadScene.NumChildren(); ++i)
                printf("Child name: %s\n", loadScene.Child(i)->Name().CString());
        }
        else
            printf("Failed to load scene from binary data\n");
    }

    return 0;
}