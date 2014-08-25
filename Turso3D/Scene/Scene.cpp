// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../IO/Deserializer.h"
#include "../IO/File.h"
#include "../IO/Serializer.h"
#include "../Object/ObjectResolver.h"
#include "../Resource/JSONFile.h"
#include "Octree.h"
#include "OctreeNode.h"
#include "Scene.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Scene::Scene() :
    nextNodeId(1)
{
    // Register self to allow finding by ID
    AddNode(this);
}

Scene::~Scene()
{
    // Node destructor will also destroy children. But at that point the node<>id maps have been destroyed 
    // so must destroy the scene tree already here
    DestroyAllChildren();
    RemoveNode(this);
    assert(nodes.IsEmpty());
}

void Scene::RegisterObject()
{
    RegisterFactory<Scene>();
    CopyBaseAttributes<Scene, Node>();
}

void Scene::Save(Serializer& dest)
{
    PROFILE(SaveScene);
    
    String fileName = FileName(dest);
    if (!fileName.IsEmpty())
        LOGINFO("Saving scene to " + fileName);
    
    /// \todo Write file ID
    Node::Save(dest);
}

bool Scene::Load(Deserializer& source)
{
    PROFILE(LoadScene);
    
    String fileName = FileName(source);
    if (!fileName.IsEmpty())
        LOGINFO("Loading scene from " + fileName);
    
    /// \todo Read file ID
    StringHash ownType = source.Read<StringHash>();
    unsigned ownId = source.Read<unsigned>();
    if (ownType != TypeStatic())
    {
        LOGERROR("Mismatching type of scene root node in scene file");
        return false;
    }

    Clear();

    ObjectResolver resolver;
    resolver.StoreObject(ownId, this);
    Node::Load(source, &resolver);
    resolver.Resolve();

    return true;
}

bool Scene::LoadJSON(const JSONValue& source)
{
    PROFILE(LoadSceneJSON);
    
    StringHash ownType(source["type"].GetString());
    unsigned ownId = (unsigned)source["id"].GetNumber();

    if (ownType != TypeStatic())
    {
        LOGERROR("Mismatching type of scene root node in scene file");
        return false;
    }

    Clear();

    ObjectResolver resolver;
    resolver.StoreObject(ownId, this);
    Node::LoadJSON(source, &resolver);
    resolver.Resolve();

    return true;
}

bool Scene::LoadJSON(Deserializer& source)
{
    String fileName = FileName(source);
    if (!fileName.IsEmpty())
        LOGINFO("Loading scene from " + fileName);
    
    JSONFile json;
    bool success = json.Load(source);
    LoadJSON(json.Root());
    return success;
}

bool Scene::SaveJSON(Serializer& dest)
{
    PROFILE(SaveSceneJSON);
    
    String fileName = FileName(dest);
    if (!fileName.IsEmpty())
        LOGINFO("Saving scene to " + fileName);
    
    JSONFile json;
    Node::SaveJSON(json.Root());
    return json.Save(dest);
}

Node* Scene::Instantiate(Deserializer& source)
{
    PROFILE(Instantiate);
    
    ObjectResolver resolver;
    StringHash childType(source.Read<StringHash>());
    unsigned childId = source.Read<unsigned>();

    Node* child = CreateChild(childType);
    if (child)
    {
        resolver.StoreObject(childId, child);
        child->Load(source, &resolver);
        resolver.Resolve();
    }

    return child;
}

Node* Scene::InstantiateJSON(const JSONValue& source)
{
    PROFILE(InstantiateJSON);
    
    ObjectResolver resolver;
    StringHash childType(source["type"].GetString());
    unsigned childId = (unsigned)source["id"].GetNumber();

    Node* child = CreateChild(childType);
    if (child)
    {
        resolver.StoreObject(childId, child);
        child->LoadJSON(source, &resolver);
        resolver.Resolve();
    }

    return child;
}

Node* Scene::InstantiateJSON(Deserializer& source)
{
    JSONFile json;
    json.Load(source);
    return InstantiateJSON(json.Root());
}

void Scene::Clear()
{
    DestroyAllChildren();
    nextNodeId = 1;
}

Node* Scene::FindNode(unsigned id) const
{
    HashMap<unsigned, Node*>::ConstIterator it = nodes.Find(id);
    return it != nodes.End() ? it->second : (Node*)0;
}

void Scene::AddNode(Node* node)
{
    if (!node || node->ParentScene() == this)
        return;

    while (nodes.Contains(nextNodeId))
    {
        ++nextNodeId;
        if (!nextNodeId)
            ++nextNodeId;
    }

    Scene* oldScene = node->ParentScene();
    if (oldScene)
    {
        unsigned oldId = node->Id();
        oldScene->nodes.Erase(oldId);
    }
    nodes[nextNodeId] = node;
    node->SetScene(this);
    node->SetId(nextNodeId);

    ++nextNodeId;

    // If node has children, add them to the scene as well
    if (node->NumChildren())
    {
        const Vector<Node*>& children = node->Children();
        for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
            AddNode(*it);
    }
}

void Scene::RemoveNode(Node* node)
{
    if (!node || node->ParentScene() != this)
        return;

    nodes.Erase(node->Id());
    node->SetScene(0);
    node->SetId(0);
    
    // If node has children, remove them from the scene as well
    if (node->NumChildren())
    {
        const Vector<Node*>& children = node->Children();
        for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
            RemoveNode(*it);
    }
}

void RegisterSceneLibrary()
{
    Node::RegisterObject();
    Scene::RegisterObject();
    SpatialNode::RegisterObject();
    OctreeNode::RegisterObject();
    Octree::RegisterObject();
}

}