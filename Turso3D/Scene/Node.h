// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/Serializable.h"
#include "../Math/Quaternion.h"

class Node;
class Octree;
class Scene;
class ObjectResolver;

static const unsigned char NF_ENABLED = 0x1;
static const unsigned char NF_TEMPORARY = 0x2;
static const unsigned char NF_SPATIAL = 0x4;
static const unsigned char NF_SPATIAL_PARENT = 0x8;
static const unsigned char NF_WORLD_TRANSFORM_DIRTY = 0x10;

static const unsigned char LAYER_DEFAULT = 0x0;
static const unsigned LAYERMASK_ALL = 0xffffffff;

/// Less time-critical implementation part to speed up linear processing of renderable nodes.
struct NodeImpl
{
    /// Parent scene.
    Scene* scene;
    /// Id within the scene.
    unsigned id;
    /// %Node name.
    std::string name;
    /// &Node name hash.
    StringHash nameHash;
};

/// Base class for scene nodes.
class Node : public Serializable
{
    OBJECT(Node);
    
public:
    /// Construct.
    Node();
    /// Destruct. Destroy any child nodes.
    ~Node();
    
    /// Register factory and attributes.
    static void RegisterObject();
    
    /// Load from binary stream. Store node references to be resolved later.
    void Load(Stream& source, ObjectResolver& resolver) override;
    /// Save to binary stream.
    void Save(Stream& dest) override;
    /// Load from JSON data. Store node references to be resolved later.
    void LoadJSON(const JSONValue& source, ObjectResolver& resolver) override;
    /// Save as JSON data.
    void SaveJSON(JSONValue& dest) override;
    /// Return unique id within the scene, or 0 if not in a scene.
    unsigned Id() const override { return impl->id; }

    /// Save as JSON text data to a binary stream. Return true on success.
    bool SaveJSON(Stream& dest);
    /// Set name. Is not required to be unique within the scene.
    void SetName(const std::string& newName);
    /// Set name.
    void SetName(const char* newName);
    /// Set node's layer. Usage is subclass specific, for example rendering nodes selectively. Default is 0.
    void SetLayer(unsigned char newLayer);
    /// Set enabled status. Meaning is subclass specific.
    void SetEnabled(bool enable);
    /// Set enabled status recursively in the child hierarchy.
    void SetEnabledRecursive(bool enable);
    /// Set temporary mode. Temporary scene nodes are not saved.
    void SetTemporary(bool enable);
    /// Reparent the node.
    void SetParent(Node* newParent);
    /// Create child node of specified type. A registered object factory for the type is required.
    Node* CreateChild(StringHash childType);
    /// Create named child node of specified type.
    Node* CreateChild(StringHash childType, const std::string& childName);
    /// Create named child node of specified type.
    Node* CreateChild(StringHash childType, const char* childName);
    /// Add node as a child. Same as calling SetParent for the child node.
    void AddChild(Node* child);
    /// Remove child node. Will delete it if there are no other strong references to it.
    void RemoveChild(Node* child);
    /// Remove child node by index.
    void RemoveChild(size_t index);
    /// Remove all child nodes.
    void RemoveAllChildren();
    /// Remove self from the parent node. No-op if no parent. Potentially causes deletion of self, if no other strong references exist.
    void RemoveSelf();
    /// Create child node of the specified type, template version.
    template <class T> T* CreateChild() { return static_cast<T*>(CreateChild(T::TypeStatic())); }
    /// Create named child node of the specified type, template version.
    template <class T> T* CreateChild(const std::string& childName) { return static_cast<T*>(CreateChild(T::TypeStatic(), childName)); }
    /// Create named child node of the specified type, template version.
    template <class T> T* CreateChild(const char* childName) { return static_cast<T*>(CreateChild(T::TypeStatic(), childName)); }

    /// Return name.
    const std::string& Name() const { return impl->name; }
    /// Return hash of name.
    const StringHash& NameHash() const { return impl->nameHash; }
    /// Return layer.
    unsigned char Layer() const { return layer; }
    /// Return bitmask corresponding to layer.
    unsigned LayerMask() const { return 1 << layer; }
    /// Return enabled status.
    bool IsEnabled() const { return TestFlag(NF_ENABLED); }
    /// Return whether is temporary.
    bool IsTemporary() const { return TestFlag(NF_TEMPORARY); }
    /// Return parent node.
    Node* Parent() const { return parent; }
    /// Return the scene that the node belongs to.
    Scene* ParentScene() const { return impl->scene; }
    /// Return number of immediate child nodes.
    size_t NumChildren() const { return children.size(); }
    /// Return number of immediate child nodes that are not temporary.
    size_t NumPersistentChildren() const;
    /// Return immediate child node by index.
    Node* Child(size_t index) const { return index < children.size() ? children[index] : nullptr; }
    /// Return all immediate child nodes.
    const std::vector<SharedPtr<Node> >& Children() const { return children; }
    /// Return child nodes recursively.
    void FindAllChildren(std::vector<Node*>& result) const;
    /// Return first child node that matches name.
    Node* FindChild(const std::string& childName, bool recursive = false) const;
    /// Return first child node that matches name.
    Node* FindChild(const char* childName, bool recursive = false) const;
    /// Return first child node that matches name hash.
    Node* FindChild(StringHash nameHash, bool recursive = false) const;
    /// Return first child node of specified type.
    Node* FindChildOfType(StringHash childType, bool recursive = false) const;
    /// Return first child node that matches type and name.
    Node* FindChildOfType(StringHash childType, const std::string& childName, bool recursive = false) const;
    /// Return first child node that matches type and name.
    Node* FindChildOfType(StringHash childType, const char* childName, bool recursive = false) const;
    /// Return first child node that matches type and name.
    Node* FindChildOfType(StringHash childType, StringHash childNameHash, bool recursive = false) const;
    /// Return first child node that matches layer mask.
    Node* FindChildByLayer(unsigned layerMask, bool recursive = false) const;
    /// Find child nodes of specified type.
    void FindChildren(std::vector<Node*>& result, StringHash childType, bool recursive = false) const;
    /// Find child nodes that match layer mask.
    void FindChildrenByLayer(std::vector<Node*>& result, unsigned layerMask, bool recursive = false) const;
    /// Return first child node of specified type, template version.
    template <class T> T* FindChild(bool recursive = false) const { return static_cast<T*>(FindChildOfType(T::TypeStatic(), recursive)); }
    /// Return first child node that matches type and name, template version.
    template <class T> T* FindChild(const std::string& childName, bool recursive = false) const { return static_cast<T*>(FindChildOfType(T::TypeStatic(), childName, recursive)); }
    /// Return first child node that matches type and name, template version.
    template <class T> T* FindChild(const char* childName, bool recursive = false) const { return static_cast<T*>(FindChildOfType(T::TypeStatic(), childName, recursive)); }
    /// Return first child node that matches type and name hash, template version.
    template <class T> T* FindChild(StringHash childNameHash, bool recursive = false) const { return static_cast<T*>(FindChildOfType(T::TypeStatic(), childNameHash, recursive)); }
    /// Find child nodes of specified type, template version.
    template <class T> void FindChildren(std::vector<T*>& result, bool recursive = false) const { return FindChildren(reinterpret_cast<std::vector<Node*>&>(result), T::TypeStatic(), recursive); }
    
    /// Set bit flag. Called internally.
    void SetFlag(unsigned char bit, bool set) const { if (set) flags |= bit; else flags &= ~bit; }
    /// Test bit flag. Called internally.
    bool TestFlag(unsigned char bit) const { return (flags & bit) != 0; }
    /// Return bit flags.
    unsigned char Flags() const { return flags; }
    /// Assign node to a new scene. Called internally.
    void SetScene(Scene* newScene);
    /// Assign new id. Called internally.
    void SetId(unsigned newId);
    
    /// Skip the binary data of a node hierarchy, in case the node could not be created.
    static void SkipHierarchy(Stream& source);

protected:
    /// Handle being assigned to a new parent node.
    virtual void OnParentSet(Node* newParent, Node* oldParent);
    /// Handle being assigned to a new scene.
    virtual void OnSceneSet(Scene* newScene, Scene* oldScene);
    /// Handle the enabled status changing.
    virtual void OnEnabledChanged(bool newEnabled);
    /// Handle the layer changing.
    virtual void OnLayerChanged(unsigned char newLayer);

private:
    /// Node implementation.
    NodeImpl* impl;
    /// Parent node.
    Node* parent;
    /// %Node flags. Used to hold several boolean values to reduce memory use.
    mutable unsigned char flags;
    /// Layer number.
    unsigned char layer;

protected:
    /// Child nodes.
    std::vector<SharedPtr<Node> > children;
};

