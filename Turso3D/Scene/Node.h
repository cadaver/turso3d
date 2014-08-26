// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/Serializable.h"

namespace Turso3D
{

class Scene;
class ObjectResolver;

static const unsigned short NF_ENABLED = 0x1;
static const unsigned short NF_TEMPORARY = 0x2;
static const unsigned short NF_SPATIAL = 0x4;
static const unsigned short NF_SPATIAL_PARENT = 0x8;
static const unsigned short NF_WORLD_TRANSFORM_DIRTY = 0x10;
static const unsigned short NF_BOUNDING_BOX_DIRTY = 0x20;
static const unsigned short NF_OCTREE_UPDATE_QUEUED = 0x40;
static const unsigned short NF_GEOMETRY = 0x80;
static const unsigned short NF_LIGHT = 0x100;
static const unsigned char LAYER_DEFAULT = 0x0;
static const unsigned LAYERMASK_ALL = 0xffffffff;

/// Base class for scene nodes.
class TURSO3D_API Node : public Serializable
{
    OBJECT(Node);
    
public:
    /// Construct.
    Node();
    /// Destruct. Destroy any child nodes.
    virtual ~Node();
    
    /// Register factory and attributes.
    static void RegisterObject();
    
    /// Load from binary stream. Store node references to be resolved later.
    virtual void Load(Deserializer& source, ObjectResolver* resolver = 0);
    /// Save to binary stream.
    virtual void Save(Serializer& dest);
    /// Load from JSON data. Store node references to be resolved later.
    virtual void LoadJSON(const JSONValue& source, ObjectResolver* resolver = 0);
    /// Save as JSON data.
    virtual void SaveJSON(JSONValue& dest);
    /// Return unique id within the scene, or 0 if not in a scene.
    virtual unsigned Id() const { return id; }

    /// Save as JSON text data to a binary stream. Return true on success.
    bool SaveJSON(Serializer& dest);
    /// Set name. Is not required to be unique within the scene.
    void SetName(const String& newName);
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
    Node* CreateChild(StringHash childType, const String& childName);
    /// Create named child node of specified type.
    Node* CreateChild(StringHash childType, const char* childName);
    /// Add node as a child. Same as calling SetParent for the child node.
    void AddChild(Node* child);
    /// Detach and return child node. The child node is removed from the scene and its deletion becomes the responsibility of the caller.
    Node* DetachChild(Node* child);
    /// Detach and return child node by index.
    Node* DetachChild(size_t);
    /// Destroy child node.
    void DestroyChild(Node* child);
    /// Destroy child node by index.
    void DestroyChild(size_t index);
    /// Destroy all child nodes.
    void DestroyAllChildren();
    /// Destroy self immediately. No operations on the node are valid after calling this.
    void DestroySelf();
    /// Create child node of the specified type, template version.
    template <class T> T* CreateChild() { return static_cast<T*>(CreateChild(T::TypeStatic())); }
    /// Create named child node of the specified type, template version.
    template <class T> T* CreateChild(const String& childName) { return static_cast<T*>(CreateChild(T::TypeStatic(), childName)); }
    /// Create named child node of the specified type, template version.
    template <class T> T* CreateChild(const char* childName) { return static_cast<T*>(CreateChild(T::TypeStatic(), childName)); }

    /// Return name.
    const String& Name() const { return name; }
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
    Scene* ParentScene() const { return scene; }
    /// Return number of immediate child nodes.
    size_t NumChildren() const { return children.Size(); }
    /// Return number of immediate child nodes that are not temporary.
    size_t NumPersistentChildren() const;
    /// Return immediate child node by index.
    Node* Child(size_t index) const { return index < children.Size() ? children[index] : (Node*)0; }
    /// Return all child nodes.
    const Vector<Node*>& Children() const { return children; }
    /// Return child nodes recursively.
    void ChildrenRecursive(Vector<Node*>& dest);
    /// Find immediate child node by name.
    Node* FindChild(const String& childName) const;
    /// Find immediate child node by name.
    Node* FindChild(const char* childName) const;
    /// Find first immediate child node of specified type.
    Node* FindChild(StringHash childType) const;
    /// Find immediate child node by type and name.
    Node* FindChild(StringHash childType, const String& childName) const;
    /// Find immediate child node by type and name.
    Node* FindChild(StringHash childType, const char* childName) const;
    /// Find child node by name recursively.
    Node* FindChildRecursive(const String& childName) const;
    /// Find child node by name recursively.
    Node* FindChildRecursive(const char* childName) const;
    /// Find child node by type recursively.
    Node* FindChildRecursive(StringHash childType) const;
    /// Find child node by type and name recursively.
    Node* FindChildRecursive(StringHash childType, const String& childName) const;
    /// Find child node by type and name recursively.
    Node* FindChildRecursive(StringHash childType, const char* childName) const;
    /// Find immediate child node by type, template version.
    template <class T> T* FindChild() const { return static_cast<T*>(FindChild(T::TypeStatic())); }
    /// Find immediate child node by type and name, template version.
    template <class T> T* FindChild(const String& childName) const { return static_cast<T*>(FindChild(T::TypeStatic(), childName)); }
    /// Find immediate child node by type and name, template version.
    template <class T> T* FindChild(const char* childName) const { return static_cast<T*>(FindChild(T::TypeStatic(), childName)); }
    /// Find child node by type recursively, template version.
    template <class T> T* FindChildRecursive() const { return static_cast<T*>(FindChildRecursive(T::TypeStatic())); }
    /// Find child node by type and name recursively, template version.
    template <class T> T* FindChildRecursive(const String& childName) const { return static_cast<T*>(FindChildRecursive(T::TypeStatic(), childName)); }
    /// Find child node by type and name recursively, template version.
    template <class T> T* FindChildRecursive(const char* childName) const { return static_cast<T*>(FindChildRecursive(T::TypeStatic(), childName)); }

    /// Set bit flag. Called internally.
    void SetFlag(unsigned short bit, bool set) const { if (set) flags |= bit; else flags &= ~bit; }
    /// Test bit flag. Called internally.
    bool TestFlag(unsigned short bit) const { return (flags & bit) != 0; }
    /// Return bit flags. Used internally eg. by octree queries.
    unsigned short Flags() const { return flags; }
    /// Assign node to a new scene. Called internally.
    void SetScene(Scene* newScene);
    /// Assign new id. Called internally.
    void SetId(unsigned newId);
    
    /// Skip the binary data of a node hierarchy, in case the node could not be created.
    static void SkipHierarchy(Deserializer& source);

protected:
    /// Handle being assigned to a new parent node.
    virtual void OnParentSet(Node* newParent, Node* oldParent);
    /// Handle being assigned to a new scene.
    virtual void OnSceneSet(Scene* newScene, Scene* oldScene);
    /// Handle the enabled status changing.
    virtual void OnSetEnabled(bool newEnabled);

private:
    /// %Node flags. Used to hold several boolean values (some subclass-specific) to reduce memory use.
    mutable unsigned short flags;
    /// Layer number.
    unsigned char layer;
    /// Parent node.
    Node* parent;
    /// Parent scene.
    Scene* scene;
    /// Child nodes. A node owns its children and destroys them during its own destruction.
    Vector<Node*> children;
    /// Id within the scene.
    unsigned id;
    /// %Node name.
    String name;
};

}
