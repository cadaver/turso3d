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
    printf("Size of OctreeNode: %d\n", sizeof(OctreeNode));
    printf("Size of Octant: %d\n", sizeof(Octant));
    
    RegisterSceneLibrary();

    printf("\nTesting scene serialization\n");
    
    Log log;
    log.Open("03_Scene.log");
    
    {
        Profiler profiler;
        profiler.BeginFrame();

        Scene scene;
        for (size_t i = 0; i < 10; ++i)
        {
            SpatialNode* node = scene.CreateChild<SpatialNode>("Child" + String(i));
            node->SetPosition(Vector3(Random(-100.0f, 100.0f), Random(-100.0f, 100.0f), Random(-100.0f, 100.0f)));
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
                    printf("Child name: %s\n", loadScene.Child(i)->Name().CString());
            }
            else
                printf("Failed to load scene from binary data\n");
        }

        profiler.EndFrame();
        LOGRAW(profiler.OutputResults(false, false, 16));
    }
    
    printf("\nTesting large object count & octree\n");

    {
        Profiler profiler;
        profiler.BeginFrame();
        
        Vector<OctreeNode*> boxNodes;
        Scene scene;
        Octree* octree = scene.CreateChild<Octree>();
        
        {
            PROFILE(CreateObjects);
            for (int y = -125; y < 125; ++y)
            {
                for (int x = -125; x < 125; ++x)
                {
                    OctreeNode* boxNode = scene.CreateChild<OctreeNode>("Box");
                    boxNode->SetPosition(Vector3(x * 0.3f, 0.0f, y * 0.3f));
                    boxNode->SetScale(0.25f);
                    boxNodes.Push(boxNode);
                }
            }
        }

        {
            PROFILE(InitialUpdateOctree);
            octree->Update();
        }

        {
            PROFILE(RotateObjects);
            Quaternion rotQuat(0.0f, 10.0f, 0.0f);
            for (size_t i = 0; i < boxNodes.Size(); ++i)
                boxNodes[i]->Rotate(rotQuat);
        }

        {
            PROFILE(RotateUpdateOctree);
            octree->Update();
        }

        {
            Vector<OctreeNode*> dest;
            BoundingBox queryBox(-10.0f, 10.0f);
            octree->FindNodes(dest, queryBox, NF_SPATIAL);
            printf("Query found %d nodes\n", (int)dest.Size());
        }
        
        {
            PROFILE(DestroyScene);
            scene.Clear();
        }

        profiler.EndFrame();
        LOGRAW(profiler.OutputResults(false, false, 16));
    }
    
    return 0;
}