// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../IO/Stream.h"
#include "../Object/ObjectResolver.h"
#include "../Resource/JSONFile.h"
#include "Scene.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static Vector<Node*> noChildren;

Node::Node() :
    flags(NF_ENABLED),
    layer(LAYER_DEFAULT),
    tag(TAG_NONE),
    parent(nullptr),
    scene(nullptr),
    id(0)
{
}

Node::~Node()
{
    DestroyAllChildren();
    // At the time of destruction the node should not have a parent, or be in a scene
    assert(!parent);
    assert(!scene);
}

void Node::RegisterObject()
{
    RegisterFactory<Node>();
    RegisterRefAttribute("name", &Node::Name, &Node::SetName);
    RegisterAttribute("enabled", &Node::IsEnabled, &Node::SetEnabled, true);
    RegisterAttribute("temporary", &Node::IsTemporary, &Node::SetTemporary, false);
    RegisterAttribute("layer", &Node::Layer, &Node::SetLayer, LAYER_DEFAULT);
    RegisterAttribute("tag", &Node::Tag, &Node::SetTag, TAG_NONE);
}

void Node::Load(Stream& source, ObjectResolver& resolver)
{
    // Type and id has been read by the parent
    Serializable::Load(source, resolver);

    size_t numChildren = source.ReadVLE();
    for (size_t i = 0; i < numChildren; ++i)
    {
        StringHash childType(source.Read<StringHash>());
        unsigned childId = source.Read<unsigned>();
        Node* child = CreateChild(childType);
        if (child)
        {
            resolver.StoreObject(childId, child);
            child->Load(source, resolver);
        }
        else
        {
            // If child is unknown type, skip all its attributes and children
            SkipHierarchy(source);
        }
    }
}

void Node::Save(Stream& dest)
{
    // Write type and ID first, followed by attributes and child nodes
    dest.Write(Type());
    dest.Write(Id());
    Serializable::Save(dest);
    dest.WriteVLE(NumPersistentChildren());

    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (!child->IsTemporary())
            child->Save(dest);
    }
}

void Node::LoadJSON(const JSONValue& source, ObjectResolver& resolver)
{
    // Type and id has been read by the parent
    Serializable::LoadJSON(source, resolver);
    
    const JSONArray& children = source["children"].GetArray();
    if (children.Size())
    {
        for (auto it = children.Begin(); it != children.End(); ++it)
        {
            const JSONValue& childJSON = *it;
            StringHash childType(childJSON["type"].GetString());
            unsigned childId = (unsigned)childJSON["id"].GetNumber();
            Node* child = CreateChild(childType);
            if (child)
            {
                resolver.StoreObject(childId, child);
                child->LoadJSON(childJSON, resolver);
            }
        }
    }
}

void Node::SaveJSON(JSONValue& dest)
{
    dest["type"] = TypeName();
    dest["id"] = Id();
    Serializable::SaveJSON(dest);
    
    if (NumPersistentChildren())
    {
        dest["children"].SetEmptyArray();
        for (auto it = children.Begin(); it != children.End(); ++it)
        {
            Node* child = *it;
            if (!child->IsTemporary())
            {
                JSONValue childJSON;
                child->SaveJSON(childJSON);
                dest["children"].Push(childJSON);
            }
        }
    }
}

bool Node::SaveJSON(Stream& dest)
{
    JSONFile json;
    SaveJSON(json.Root());
    return json.Save(dest);
}

void Node::SetName(const String& newName)
{
    SetName(newName.CString());
}

void Node::SetName(const char* newName)
{
    name = newName;
}

void Node::SetLayer(unsigned char newLayer)
{
    if (layer < 32)
        layer = newLayer;
    else
        LOGERROR("Can not set layer 32 or higher");
}

void Node::SetLayerName(const String& newLayerName)
{
    if (!scene)
        return;
    
    const HashMap<String, unsigned char>& layers = scene->Layers();
    auto it = layers.Find(newLayerName);
    if (it != layers.End())
        layer = it->second;
    else
        LOGERROR("Layer " + newLayerName + " not defined in the scene");
}

void Node::SetTag(unsigned char newTag)
{
    tag = newTag;
}

void Node::SetTagName(const String& newTagName)
{
    if (!scene)
        return;

    const HashMap<String, unsigned char>& tags = scene->Tags();
    auto it = tags.Find(newTagName);
    if (it != tags.End())
        tag = it->second;
    else
        LOGERROR("Tag " + newTagName + " not defined in the scene");
}

void Node::SetEnabled(bool enable)
{
    SetFlag(NF_ENABLED, enable);
    OnSetEnabled(TestFlag(NF_ENABLED));
}

void Node::SetEnabledRecursive(bool enable)
{
    SetEnabled(enable);
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        child->SetEnabledRecursive(enable);
    }
}

void Node::SetTemporary(bool enable)
{
    SetFlag(NF_TEMPORARY, enable);
}

void Node::SetParent(Node* newParent)
{
    if (newParent)
        newParent->AddChild(this);
    else
        LOGERROR("Could not set null parent");
}

Node* Node::CreateChild(StringHash childType)
{
    AutoPtr<Object> newObject = Create(childType);
    if (!newObject)
    {
        LOGERROR("Could not create child node of unknown type " + childType.ToString());
        return 0;
    }
    Node* child = dynamic_cast<Node*>(newObject.Get());
    if (!child)
    {
        LOGERROR(newObject->TypeName() + " is not a Node subclass, could not add as a child");
        return 0;
    }

    newObject.Detach();
    AddChild(child);
    return child;
}

Node* Node::CreateChild(StringHash childType, const String& childName)
{
    return CreateChild(childType, childName.CString());
}

Node* Node::CreateChild(StringHash childType, const char* childName)
{
    Node* child = CreateChild(childType);
    if (child)
        child->SetName(childName);
    return child;
}

void Node::AddChild(Node* child)
{
    // Check for illegal or redundant parent assignment
    if (!child || child->parent == this)
        return;
    
    if (child == this)
    {
        LOGERROR("Attempted parenting node to self");
        return;
    }

    // Check for possible cyclic parent assignment
    Node* current = parent;
    while (current)
    {
        if (current == child)
        {
            LOGERROR("Attempted cyclic node parenting");
            return;
        }
        current = current->parent;
    }

    Node* oldParent = child->parent;
    if (oldParent)
        oldParent->children.Remove(child);
    children.Push(child);
    child->parent = this;
    child->OnParentSet(this, oldParent);
    if (scene)
        scene->AddNode(child);
}

Node* Node::DetachChild(Node* child)
{
    if (!child || child->parent != this)
        return 0;

    for (size_t i = 0; i < children.Size(); ++i)
    {
        if (children[i] == child)
            return DetachChild(i);
    }

    return 0;
}

Node* Node::DetachChild(size_t index)
{
    if (index >= children.Size())
        return 0;

    Node* child = children[index];
    children.Erase(index);
    // Detach from both the parent and the scene (removes id assignment)
    child->parent = nullptr;
    child->OnParentSet(this, 0);
    if (scene)
        scene->RemoveNode(child);
    return child;
}

void Node::DestroyChild(Node* child)
{
    child = DetachChild(child);
    delete child;
}

void Node::DestroyChild(size_t index)
{
    Node* child = DetachChild(index);
    delete child;
}

void Node::DestroyAllChildren()
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        child->parent = nullptr;
        child->OnParentSet(this, 0);
        if (scene)
            scene->RemoveNode(child);
        delete child;
        *it = nullptr;
    }
    
    children.Clear();
}

void Node::DestroySelf()
{
    if (parent)
        parent->DestroyChild(this);
    else
        delete this;
}

const String& Node::LayerName() const
{
    if (!scene)
        return String::EMPTY;

    const Vector<String>& layerNames = scene->LayerNames();
    return layer < layerNames.Size() ? layerNames[layer] : String::EMPTY;
}

const String& Node::TagName() const
{
    if (!scene)
        return String::EMPTY;

    const Vector<String>& tagNames = scene->TagNames();
    return tag < tagNames.Size() ? tagNames[layer] : String::EMPTY;
}

size_t Node::NumPersistentChildren() const
{
    size_t ret = 0;

    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (!child->IsTemporary())
            ++ret;
    }

    return ret;
}

void Node::AllChildren(Vector<Node*>& result) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        result.Push(child);
        child->AllChildren(result);
    }
}

Node* Node::FindChild(const String& childName, bool recursive) const
{
    return FindChild(childName.CString(), recursive);
}

Node* Node::FindChild(const char* childName, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->name == childName)
            return child;
        else if (recursive && child->children.Size())
        {
            Node* childResult = child->FindChild(childName, recursive);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

Node* Node::FindChild(StringHash childType, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType)
            return child;
        else if (recursive && child->children.Size())
        {
            Node* childResult = child->FindChild(childType, recursive);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

Node* Node::FindChild(StringHash childType, const String& childName, bool recursive) const
{
    return FindChild(childType, childName.CString(), recursive);
}

Node* Node::FindChild(StringHash childType, const char* childName, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType && child->name == childName)
            return child;
        else if (recursive && child->children.Size())
        {
            Node* childResult = child->FindChild(childType, childName, recursive);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

Node* Node::FindChildByLayer(unsigned layerMask, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->LayerMask() && layerMask)
            return child;
        else if (recursive && child->children.Size())
        {
            Node* childResult = child->FindChildByLayer(layerMask, recursive);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

Node* Node::FindChildByTag(unsigned char tag_, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->tag == tag_)
            return child;
        else if (recursive && child->children.Size())
        {
            Node* childResult = child->FindChildByTag(tag_, recursive);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

Node* Node::FindChildByTag(const String& tagName, bool recursive) const
{
    return FindChildByTag(tagName.CString(), recursive);
}

Node* Node::FindChildByTag(const char* tagName, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (!String::Compare(child->TagName().CString(), tagName))
            return child;
        else if (recursive && child->children.Size())
        {
            Node* childResult = child->FindChildByTag(tagName, recursive);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

void Node::FindChildren(Vector<Node*>& result, StringHash childType, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType)
            result.Push(child);
        if (recursive && child->children.Size())
            child->FindChildren(result, childType, recursive);
    }
}

void Node::FindChildrenByLayer(Vector<Node*>& result, unsigned layerMask, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->LayerMask() & layerMask)
            result.Push(child);
        if (recursive && child->children.Size())
            child->FindChildrenByLayer(result, layerMask, recursive);
    }
}

void Node::FindChildrenByTag(Vector<Node*>& result, unsigned char tag_, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->tag == tag_)
            result.Push(child);
        if (recursive && child->children.Size())
            child->FindChildrenByTag(result, tag_, recursive);
    }
}

void Node::FindChildrenByTag(Vector<Node*>& result, const String& tagName, bool recursive) const
{
    FindChildrenByTag(result, tagName.CString(), recursive);
}

void Node::FindChildrenByTag(Vector<Node*>& result, const char* tagName, bool recursive) const
{
    for (auto it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (!String::Compare(child->TagName().CString(), tagName))
            result.Push(child);
        if (recursive && child->children.Size())
            child->FindChildrenByTag(result, tagName, recursive);
    }
}

size_t Node::NumSiblings() const
{
    return parent ? parent->NumChildren() : 0;
}

Node* Node::Sibling(size_t index) const
{
    return parent ? parent->Child(index) : 0;
}

const Vector<Node*>& Node::Siblings() const
{
    return parent ? parent->children : noChildren;
}

Node* Node::FindSibling(const String& siblingName) const
{
    return parent ? parent->FindChild(siblingName) : 0;
}

Node* Node::FindSibling(const char* siblingName) const
{
    return parent ? parent->FindChild(siblingName) : 0;
}

Node* Node::FindSibling(StringHash siblingType) const
{
    return parent ? parent->FindChild(siblingType) : 0;
}

Node* Node::FindSibling(StringHash siblingType, const String& siblingName) const
{
    return parent ? parent->FindChild(siblingType, siblingName) : 0;
}

Node* Node::FindSibling(StringHash siblingType, const char* siblingName) const
{
    return parent ? parent->FindChild(siblingType, siblingName) : 0;
}

Node* Node::FindSiblingByLayer(unsigned layerMask) const
{
    return parent ? parent->FindChildByLayer(layerMask) : 0;
}

Node* Node::FindSiblingByTag(unsigned char tag) const
{
    return parent ? parent->FindChildByTag(tag) : 0;
}

Node* Node::FindSiblingByTag(const String& tagName) const
{
    return parent ? parent->FindChildByTag(tagName) : 0;
}

Node* Node::FindSiblingByTag(const char* tagName) const
{
    return parent ? parent->FindChildByTag(tagName) : 0;
}

void Node::FindSiblings(Vector<Node*>& result, StringHash siblingType) const
{
    if (parent)
        parent->FindChildren(result, siblingType);
}

void Node::FindSiblingsByLayer(Vector<Node*>& result, unsigned layerMask) const
{
    if (parent)
        parent->FindChildrenByLayer(result, layerMask);
}

void Node::FindSiblingsByTag(Vector<Node*>& result, unsigned char tag) const
{
    if (parent)
        parent->FindChildrenByTag(result, tag);
}

void Node::FindSiblingsByTag(Vector<Node*>& result, const String& tagName) const
{
    if (parent)
        parent->FindChildrenByTag(result, tagName);
}

void Node::FindSiblingsByTag(Vector<Node*>& result, const char* tagName) const
{
    if (parent)
        parent->FindChildrenByTag(result, tagName);
}

void Node::SetScene(Scene* newScene)
{
    Scene* oldScene = scene;
    scene = newScene;
    OnSceneSet(scene, oldScene);
}

void Node::SetId(unsigned newId)
{
    id = newId;
}

void Node::SkipHierarchy(Stream& source)
{
    Serializable::Skip(source);

    size_t numChildren = source.ReadVLE();
    for (size_t i = 0; i < numChildren; ++i)
    {
        source.Read<StringHash>(); // StringHash childType
        source.Read<unsigned>(); // unsigned childId
        SkipHierarchy(source);
    }
}

void Node::OnParentSet(Node*, Node*)
{
}

void Node::OnSceneSet(Scene*, Scene*)
{
}

void Node::OnSetEnabled(bool)
{
}

}
