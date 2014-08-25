// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../IO/Deserializer.h"
#include "../IO/Serializer.h"
#include "../Object/ObjectResolver.h"
#include "../Resource/JSONFile.h"
#include "Scene.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Node::Node() :
    flags(NF_ENABLED),
    parent(0),
    scene(0)
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
}

void Node::Load(Deserializer& source, ObjectResolver* resolver)
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
            if (resolver)
                resolver->StoreObject(childId, child);
            child->Load(source, resolver);
        }
        else
        {
            // If child is unknown type, skip all its attributes and children
            SkipHierarchy(source);
        }
    }
}

void Node::Save(Serializer& dest)
{
    // Write type and ID first, followed by attributes and child nodes
    dest.Write(Type());
    dest.Write(Id());
    Serializable::Save(dest);
    dest.WriteVLE(NumPersistentChildren());

    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (!child->IsTemporary())
            child->Save(dest);
    }
}

void Node::LoadJSON(const JSONValue& source, ObjectResolver* resolver)
{
    // Type and id has been read by the parent
    Serializable::LoadJSON(source);
    
    const JSONArray& children = source["children"].GetArray();
    if (children.Size())
    {
        for (JSONArray::ConstIterator it = children.Begin(); it != children.End(); ++it)
        {
            const JSONValue& childJSON = *it;
            StringHash childType(childJSON["type"].GetString());
            unsigned childId = (unsigned)childJSON["id"].GetNumber();
            Node* child = CreateChild(childType);
            if (child)
            {
                if (resolver)
                    resolver->StoreObject(childId, child);
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
        for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
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

bool Node::SaveJSON(Serializer& dest)
{
    JSONFile json;
    SaveJSON(json.Root());
    return json.Save(dest);
}

void Node::SetName(const String& newName)
{
    name = newName;
}

void Node::SetEnabled(bool enable)
{
    SetFlag(NF_ENABLED, enable);
    OnSetEnabled(TestFlag(NF_ENABLED));
}

void Node::SetEnabledRecursive(bool enable)
{
    SetEnabled(enable);
    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
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
    child->parent = 0;
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
    for (Vector<Node*>::Iterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        child->parent = 0;
        child->OnParentSet(this, 0);
        if (scene)
            scene->RemoveNode(child);
        delete child;
        *it = 0;
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

unsigned Node::Id() const
{
    // The id is not needed often. Query from Scene to avoid having to store into nodes separately
    return scene ? scene->FindNodeId(const_cast<Node*>(this)) : 0;
}

size_t Node::NumPersistentChildren() const
{
    size_t ret = 0;

    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (!child->IsTemporary())
            ++ret;
    }

    return ret;
}

void Node::ChildrenRecursive(Vector<Node*>& dest)
{
    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        dest.Push(child);
        child->ChildrenRecursive(dest);
    }
}

Node* Node::FindChild(const String& childName) const
{
    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->name == childName)
            return child;
    }

    return 0;
}

Node* Node::FindChild(StringHash childType) const
{
    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType)
            return child;
    }

    return 0;
}

Node* Node::FindChild(StringHash childType, const String& childName) const
{
    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType && child->name == childName)
            return child;
    }

    return 0;
}

Node* Node::FindChildRecursive(const String& childName) const
{
    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->name == childName)
            return child;
        else if (child->children.Size())
        {
            Node* childResult = child->FindChildRecursive(childName);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

Node* Node::FindChildRecursive(StringHash childType) const
{
    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType)
            return child;
        else if (child->children.Size())
        {
            Node* childResult = child->FindChildRecursive(childType);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

Node* Node::FindChildRecursive(StringHash childType, const String& childName) const
{
    for (Vector<Node*>::ConstIterator it = children.Begin(); it != children.End(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType && child->name == childName)
            return child;
        else if (child->children.Size())
        {
            Node* childResult = child->FindChildRecursive(childType, childName);
            if (childResult)
                return childResult;
        }
    }

    return 0;
}

void Node::SetScene(Scene* newScene)
{
    Scene* oldScene = scene;
    scene = newScene;
    OnSceneSet(scene, oldScene);
}

void Node::SkipHierarchy(Deserializer& source)
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
