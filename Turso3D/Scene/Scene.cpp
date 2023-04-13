// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../IO/Stream.h"
#include "../Object/ObjectResolver.h"
#include "../Resource/JSONFile.h"
#include "Scene.h"
#include "SpatialNode.h"

#include <tracy/Tracy.hpp>

Scene::Scene() :
    nextNodeId(1)
{
    // Register self to allow finding by ID
    AddNode(this);
}

Scene::~Scene()
{
    // Node destructor will also remove children. But at that point the node<>id maps have been destroyed so must tear down the scene tree already here
    RemoveAllChildren();
    RemoveNode(this);
    assert(nodes.empty());
}

void Scene::RegisterObject()
{
    // Register scene allocator with small initial capacity with the assumption that we don't create many of them (unlike other scene nodes)
    RegisterFactory<Scene>(1);
    RegisterDerivedType<Scene, Node>();
    CopyBaseAttributes<Scene, Node>();
}

void Scene::Save(Stream& dest)
{
    ZoneScoped;
    
    LOGINFO("Saving scene to " + dest.Name());
    
    dest.WriteFileID("SCNE");
    Node::Save(dest);
}

bool Scene::Load(Stream& source)
{
    ZoneScoped;
    
    LOGINFO("Loading scene from " + source.Name());
    
    std::string fileId = source.ReadFileID();
    if (fileId != "SCNE")
    {
        LOGERROR("File is not a binary scene file");
        return false;
    }

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
    Node::Load(source, resolver);
    resolver.Resolve();

    return true;
}

bool Scene::LoadJSON(const JSONValue& source)
{
    ZoneScoped;
    
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
    Node::LoadJSON(source, resolver);
    resolver.Resolve();

    return true;
}

bool Scene::LoadJSON(Stream& source)
{
    ZoneScoped;

    LOGINFO("Loading scene from " + source.Name());
    
    JSONFile json;
    bool success = json.Load(source);
    LoadJSON(json.Root());
    return success;
}

bool Scene::SaveJSON(Stream& dest)
{
    ZoneScoped;
    
    LOGINFO("Saving scene to " + dest.Name());
    
    JSONFile json;
    Node::SaveJSON(json.Root());
    return json.Save(dest);
}

Node* Scene::Instantiate(Stream& source)
{
    ZoneScoped;
    
    ObjectResolver resolver;
    StringHash childType(source.Read<StringHash>());
    unsigned childId = source.Read<unsigned>();

    Node* child = CreateChild(childType);
    if (child)
    {
        resolver.StoreObject(childId, child);
        child->Load(source, resolver);
        resolver.Resolve();
    }

    return child;
}

Node* Scene::InstantiateJSON(const JSONValue& source)
{
    ZoneScoped;
    
    ObjectResolver resolver;
    StringHash childType(source["type"].GetString());
    unsigned childId = (unsigned)source["id"].GetNumber();

    Node* child = CreateChild(childType);
    if (child)
    {
        resolver.StoreObject(childId, child);
        child->LoadJSON(source, resolver);
        resolver.Resolve();
    }

    return child;
}

Node* Scene::InstantiateJSON(Stream& source)
{
    ZoneScoped;

    JSONFile json;
    json.Load(source);
    return InstantiateJSON(json.Root());
}

void Scene::Clear()
{
    RemoveAllChildren();
    nextNodeId = 1;
}

Node* Scene::FindNode(unsigned id_) const
{
    auto it = nodes.find(id_);
    return it != nodes.end() ? it->second : nullptr;
}

void Scene::AddNode(Node* node)
{
    if (!node || node->ParentScene() == this)
        return;

    while (nodes.find(nextNodeId) != nodes.end())
    {
        ++nextNodeId;
        if (!nextNodeId)
            ++nextNodeId;
    }

    Scene* oldScene = node->ParentScene();
    if (oldScene)
    {
        unsigned oldId = node->Id();
        oldScene->nodes.erase(oldId);
    }

    nodes[nextNodeId] = node;
    node->SetScene(this);
    node->SetId(nextNodeId);

    ++nextNodeId;

    // If node has children, add them to the scene as well
    if (node->NumChildren())
    {
        const std::vector<SharedPtr<Node> >& nodeChildren = node->Children();
        for (auto it = nodeChildren.begin(); it != nodeChildren.end(); ++it)
            AddNode(*it);
    }
}

void Scene::RemoveNode(Node* node)
{
    if (!node || node->ParentScene() != this)
        return;

    nodes.erase(node->Id());
    node->SetScene(nullptr);
    node->SetId(0);
    
    // If node has children, remove them from the scene as well
    if (node->NumChildren())
    {
        const std::vector<SharedPtr<Node> >& nodeChildren = node->Children();
        for (auto it = nodeChildren.begin(); it != nodeChildren.end(); ++it)
            RemoveNode(*it);
    }
}

void RegisterSceneLibrary()
{
    static bool registered = false;
    if (registered)
        return;

    Node::RegisterObject();
    Scene::RegisterObject();
    SpatialNode::RegisterObject();

    registered = true;
}
