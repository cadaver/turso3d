// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../IO/Stream.h"
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

    DefineLayer(LAYER_DEFAULT, "Default");
    DefineTag(TAG_NONE, "None");
}

Scene::~Scene()
{
    // Node destructor will also destroy children. But at that point the node<>id maps have been destroyed 
    // so must tear down the scene tree already here
    DestroyAllChildren();
    RemoveNode(this);
    assert(nodes.IsEmpty());
}

void Scene::RegisterObject()
{
    RegisterFactory<Scene>();
    CopyBaseAttributes<Scene, Node>();
    RegisterAttribute("layerNames", &Scene::LayerNamesAttr, &Scene::SetLayerNamesAttr);
    RegisterAttribute("tagNames", &Scene::TagNamesAttr, &Scene::SetTagNamesAttr);
}

void Scene::Save(Stream& dest)
{
    PROFILE(SaveScene);
    
    LOGINFO("Saving scene to " + dest.Name());
    
    dest.WriteFileID("SCNE");
    Node::Save(dest);
}

bool Scene::Load(Stream& source)
{
    PROFILE(LoadScene);
    
    LOGINFO("Loading scene from " + source.Name());
    
    String fileId = source.ReadFileID();
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
    Node::LoadJSON(source, resolver);
    resolver.Resolve();

    return true;
}

bool Scene::LoadJSON(Stream& source)
{
    LOGINFO("Loading scene from " + source.Name());
    
    JSONFile json;
    bool success = json.Load(source);
    LoadJSON(json.Root());
    return success;
}

bool Scene::SaveJSON(Stream& dest)
{
    PROFILE(SaveSceneJSON);
    
    LOGINFO("Saving scene to " + dest.Name());
    
    JSONFile json;
    Node::SaveJSON(json.Root());
    return json.Save(dest);
}

Node* Scene::Instantiate(Stream& source)
{
    PROFILE(Instantiate);
    
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
    PROFILE(InstantiateJSON);
    
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
    JSONFile json;
    json.Load(source);
    return InstantiateJSON(json.Root());
}

void Scene::DefineLayer(unsigned char index, const String& name)
{
    if (index >= 32)
    {
        LOGERROR("Can not define more than 32 layers");
        return;
    }

    if (layerNames.Size() <= index)
        layerNames.Resize(index + 1);
    layerNames[index] = name;
    layers[name] = index;
}

void Scene::DefineTag(unsigned char index, const String& name)
{
    if (tagNames.Size() <= index)
        tagNames.Resize(index + 1);
    tagNames[index] = name;
    tags[name] = index;
}

void Scene::Clear()
{
    DestroyAllChildren();
    nextNodeId = 1;
}

Node* Scene::FindNode(unsigned id) const
{
    auto it = nodes.Find(id);
    return it != nodes.End() ? it->second : nullptr;
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
        for (auto it = children.Begin(); it != children.End(); ++it)
            AddNode(*it);
    }
}

void Scene::RemoveNode(Node* node)
{
    if (!node || node->ParentScene() != this)
        return;

    nodes.Erase(node->Id());
    node->SetScene(nullptr);
    node->SetId(0);
    
    // If node has children, remove them from the scene as well
    if (node->NumChildren())
    {
        const Vector<Node*>& children = node->Children();
        for (auto it = children.Begin(); it != children.End(); ++it)
            RemoveNode(*it);
    }
}

void Scene::SetLayerNamesAttr(JSONValue names)
{
    layerNames.Clear();
    layers.Clear();

    const JSONArray& array = names.GetArray();
    for (size_t i = 0; i < array.Size(); ++i)
    {
        const String& name = array[i].GetString();
        layerNames.Push(name);
        layers[name] = (unsigned char)i;
    }
}

JSONValue Scene::LayerNamesAttr() const
{
    JSONValue ret;

    ret.SetEmptyArray();
    for (auto it = layerNames.Begin(); it != layerNames.End(); ++it)
        ret.Push(*it);
    
    return ret;
}

void Scene::SetTagNamesAttr(JSONValue names)
{
    tagNames.Clear();
    tags.Clear();

    const JSONArray& array = names.GetArray();
    for (size_t i = 0; i < array.Size(); ++i)
    {
        const String& name = array[i].GetString();
        tagNames.Push(name);
        tags[name] = (unsigned char)i;
    }
}

JSONValue Scene::TagNamesAttr() const
{
    JSONValue ret;

    ret.SetEmptyArray();
    for (auto it = tagNames.Begin(); it != tagNames.End(); ++it)
        ret.Push(*it);

    return ret;
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
