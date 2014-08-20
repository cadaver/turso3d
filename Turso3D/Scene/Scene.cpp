// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../IO/Deserializer.h"
#include "../IO/JSONFile.h"
#include "../IO/Serializer.h"
#include "Scene.h"
#include "SceneResolver.h"

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
    for (HashMap<unsigned, Node*>::Iterator it = nodes.Begin(); it != nodes.End(); ++it)
        it->second->SetScene(0);
}

void Scene::RegisterObject()
{
    RegisterFactory<Scene>();
    RegisterRefAttribute("name", &Node::Name, &Node::SetName);
    RegisterAttribute("nextNodeId", &Scene::NextNodeId, &Scene::SetNextNodeId);
}

void Scene::Load(Deserializer& source)
{
    Clear();

    StringHash ownType = source.Read<StringHash>();
    if (ownType != TypeStatic())
    {
        LOGERROR("Mismatching type of scene root node in scene file");
        return;
    }

    SceneResolver resolver;
    unsigned ownId = source.Read<unsigned>();
    //resolver.AddNode(ownId, this);
    Node::Load(source, resolver);
    //resolver.Resolve();
}

void Scene::LoadJSON(const JSONValue& source)
{
    Clear();

    StringHash ownType = source["type"].GetString();
    if (ownType != TypeStatic())
    {
        LOGERROR("Mismatching type of scene root node in scene file");
        return;
    }

    SceneResolver resolver;
    unsigned ownId = (unsigned)source["id"].GetNumber();
    //resolver.AddNode(ownId, this);

    Node::LoadJSON(source, resolver);
    //resolver.Resolve();
}

void Scene::LoadJSON(Deserializer& source)
{
    JSONFile json;
    json.Load(source);
    LoadJSON(json.Root());
}

Node* Scene::Instantiate(Deserializer& source)
{
    SceneResolver resolver;
    StringHash childType = source.Read<StringHash>();
    unsigned childId = source.Read<unsigned>();

    Node* child = CreateChild(childType);
    if (child)
    {
        //resolver.AddNode(childId, child);
        child->Load(source, resolver);
        //resolver.Resolve();
    }

    return child;
}

Node* Scene::InstantiateJSON(const JSONValue& source)
{
    SceneResolver resolver;
    StringHash childType = source["type"].GetString();
    unsigned childId = (unsigned)source["id"].GetNumber();

    Node* child = CreateChild(childType);
    if (child)
    {
        //resolver.AddNode(childId, child);
        child->LoadJSON(source, resolver);
        //resolver.Resolve();
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

unsigned Scene::FindNodeId(Node* node) const
{
    HashMap<Node*, unsigned>::ConstIterator it = ids.Find(node);
    return it != ids.End() ? it->second : 0;
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
        oldScene->ids.Erase(node);
    }
    nodes[nextNodeId] = node;
    ids[node] = nextNodeId;
    node->SetScene(this);

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
    ids.Erase(node);
    node->SetScene(0);
    
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
}

}
