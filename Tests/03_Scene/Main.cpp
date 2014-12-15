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
    printf("Size of Scene: %d\n", sizeof(Scene));
    printf("Size of SpatialNode: %d\n", sizeof(SpatialNode));
    
    RegisterSceneLibrary();

    printf("\nTesting scene serialization\n");
    
    Log log;
    log.Open("03_Scene.log");
    
    {
        Profiler profiler;
        profiler.BeginFrame();

        Scene scene;
        scene.DefineLayer(1, "TestLayer");
        scene.DefineTag(1, "TestTag");
        for (size_t i = 0; i < 10; ++i)
        {
            SpatialNode* node = scene.CreateChild<SpatialNode>("Child" + String(i));
            node->SetPosition(Vector3(Random(-100.0f, 100.0f), Random(-100.0f, 100.0f), Random(-100.0f, 100.0f)));
            node->SetLayerName("TestLayer");
            node->SetTagName("TestTag");
        }

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
                {
                    Node* child = loadScene.Child(i);
                    printf("Child name: %s layer: %d tag: %d\n", child->Name().CString(), (int)child->Layer(), (int)child->Tag());
                }
            }
            else
                printf("Failed to load scene from binary data\n");
        }

        profiler.EndFrame();
        LOGRAW(profiler.OutputResults(false, false, 16));
    }
    
    return 0;
}