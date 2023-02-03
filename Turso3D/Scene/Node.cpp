// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../IO/Stream.h"
#include "../Object/Allocator.h"
#include "../Object/ObjectResolver.h"
#include "../Resource/JSONFile.h"
#include "Scene.h"

static std::vector<SharedPtr<Node> > noChildren;
static Allocator<NodeImpl> nodeImplAllocator;

Node::Node() :
    impl(nodeImplAllocator.Allocate()),
    parent(nullptr),
    flags(NF_ENABLED),
    layer(LAYER_DEFAULT)
{
    impl->scene = nullptr;
    impl->id = 0;
}

Node::~Node()
{
    RemoveAllChildren();
    // At the time of destruction the node should not have a parent, or be in a scene
    assert(!parent);
    assert(!impl->scene);

    nodeImplAllocator.Free(impl);
}

void Node::RegisterObject()
{
    RegisterFactory<Node>();
    RegisterRefAttribute("name", &Node::Name, &Node::SetName);
    RegisterAttribute("enabled", &Node::IsEnabled, &Node::SetEnabled, true);
    RegisterAttribute("temporary", &Node::IsTemporary, &Node::SetTemporary, false);
    RegisterAttribute("layer", &Node::Layer, &Node::SetLayer, LAYER_DEFAULT);
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

    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
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
    
    const JSONArray& childrenArray = source["children"].GetArray();
    if (childrenArray.size())
    {
        for (auto it = childrenArray.begin(); it != childrenArray.end(); ++it)
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
        for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
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

void Node::SetName(const std::string& newName)
{
    impl->name = newName;
}

void Node::SetName(const char* newName)
{
    impl->name = newName;
}

void Node::SetLayer(unsigned char newLayer)
{
    if (layer < 32)
        layer = newLayer;
    else
        LOGERROR("Can not set layer 32 or higher");
}

void Node::SetEnabled(bool enable)
{
    if (enable != TestFlag(NF_ENABLED))
    {
        SetFlag(NF_ENABLED, enable);
        OnEnabledChanged(enable);
    }
}

void Node::SetEnabledRecursive(bool enable)
{
    SetEnabled(enable);
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
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
    SharedPtr<Object> newObject = Create(childType);
    if (!newObject)
    {
        LOGERROR("Could not create child node of unknown type " + childType.ToString());
        return nullptr;
    }
    Node* child = dynamic_cast<Node*>(newObject.Get());
    if (!child)
    {
        LOGERROR(newObject->TypeName() + " is not a Node subclass, could not add as a child");
        return nullptr;
    }

    AddChild(child);
    return child;
}

Node* Node::CreateChild(StringHash childType, const std::string& childName)
{
    return CreateChild(childType, childName.c_str());
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
    
#ifdef _DEBUG
    // Check for possible illegal or cyclic parent assignment
    if (child == this)
    {
        LOGERROR("Attempted parenting node to self");
        return;
    }

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
#endif

    Node* oldParent = child->parent;
    if (oldParent)
    {
        for (auto it = oldParent->impl->children.begin(); it != oldParent->impl->children.end(); ++it)
        {
            if (*it == child)
            {
                oldParent->impl->children.erase(it);
                break;
            }
        }
    }

    impl->children.push_back(child);
    child->parent = this;
    child->OnParentSet(this, oldParent);
    if (impl->scene)
        impl->scene->AddNode(child);
}

void Node::RemoveChild(Node* child)
{
    if (!child || child->parent != this)
        return;

    for (size_t i = 0; i < impl->children.size(); ++i)
    {
        if (impl->children[i] == child)
        {
            RemoveChild(i);
            break;
        }
    }
}

void Node::RemoveChild(size_t index)
{
    if (index >= impl->children.size())
        return;

    Node* child = impl->children[index];
    // Detach from both the parent and the scene (removes id assignment)
    child->parent = nullptr;
    child->OnParentSet(nullptr, this);
    if (impl->scene)
        impl->scene->RemoveNode(child);
    impl->children.erase(impl->children.begin() + index);
}

void Node::RemoveAllChildren()
{
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        child->parent = nullptr;
        child->OnParentSet(nullptr, this);
        if (impl->scene)
            impl->scene->RemoveNode(child);
        it->Reset();
    }

    impl->children.clear();
}

void Node::RemoveSelf()
{
    if (parent)
        parent->RemoveChild(this);
    else
        delete this;
}

size_t Node::NumPersistentChildren() const
{
    size_t ret = 0;

    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        if (!child->IsTemporary())
            ++ret;
    }

    return ret;
}

void Node::FindAllChildren(std::vector<Node*>& result) const
{
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        result.push_back(child);
        child->FindAllChildren(result);
    }
}

Node* Node::FindChild(const std::string& childName, bool recursive) const
{
    return FindChild(childName.c_str(), recursive);
}

Node* Node::FindChild(const char* childName, bool recursive) const
{
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        if (child->impl->name == childName)
            return child;
        else if (recursive && child->impl->children.size())
        {
            Node* childResult = child->FindChild(childName, recursive);
            if (childResult)
                return childResult;
        }
    }

    return nullptr;
}

Node* Node::FindChild(StringHash childType, bool recursive) const
{
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType || DerivedFrom(child->Type(), childType))
            return child;
        else if (recursive && child->impl->children.size())
        {
            Node* childResult = child->FindChild(childType, recursive);
            if (childResult)
                return childResult;
        }
    }

    return nullptr;
}

Node* Node::FindChild(StringHash childType, const std::string& childName, bool recursive) const
{
    return FindChild(childType, childName.c_str(), recursive);
}

Node* Node::FindChild(StringHash childType, const char* childName, bool recursive) const
{
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        if ((child->Type() == childType || DerivedFrom(child->Type(), childType)) && child->impl->name == childName)
            return child;
        else if (recursive && child->impl->children.size())
        {
            Node* childResult = child->FindChild(childType, childName, recursive);
            if (childResult)
                return childResult;
        }
    }

    return nullptr;
}

Node* Node::FindChildByLayer(unsigned layerMask, bool recursive) const
{
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        if (child->LayerMask() && layerMask)
            return child;
        else if (recursive && child->impl->children.size())
        {
            Node* childResult = child->FindChildByLayer(layerMask, recursive);
            if (childResult)
                return childResult;
        }
    }

    return nullptr;
}

void Node::FindChildren(std::vector<Node*>& result, StringHash childType, bool recursive) const
{
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        if (child->Type() == childType || DerivedFrom(child->Type(), childType))
            result.push_back(child);
        if (recursive && child->impl->children.size())
            child->FindChildren(result, childType, recursive);
    }
}

void Node::FindChildrenByLayer(std::vector<Node*>& result, unsigned layerMask, bool recursive) const
{
    for (auto it = impl->children.begin(); it != impl->children.end(); ++it)
    {
        Node* child = *it;
        if (child->LayerMask() & layerMask)
            result.push_back(child);
        if (recursive && child->impl->children.size())
            child->FindChildrenByLayer(result, layerMask, recursive);
    }
}

void Node::SetScene(Scene* newScene)
{
    Scene* oldScene = impl->scene;
    impl->scene = newScene;
    OnSceneSet(impl->scene, oldScene);
}

void Node::SetId(unsigned newId)
{
    impl->id = newId;
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

void Node::OnEnabledChanged(bool)
{
}
