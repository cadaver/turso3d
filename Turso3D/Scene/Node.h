// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/Serializable.h"

namespace Turso3D
{

class Scene;
class SceneResolver;

static const unsigned NF_ENABLED = 0x1;
static const unsigned NF_TEMPORARY = 0x2;

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
    
    /// Save to a binary stream.
    virtual void Save(Serializer& dest);
    /// Save to JSON data.
    virtual void SaveJSON(JSONValue& dest);
    /// Save JSON data as text to a binary stream. Return true on success.
    bool SaveJSON(Serializer& dest);
    
    /// Load from a binary stream. Leave other scene node references to be resolved later.
    void Load(Deserializer& source, SceneResolver& resolver);
    /// Load from JSON data. Leave other scene node references to be resolved later.
    void LoadJSON(const JSONValue& source, SceneResolver& resolver);
    
    /// Set name, which is not required to be unique within the scene.
    void SetName(const String& newName);
    /// Set enabled status. Meaning is subclass specific.
    void SetEnabled(bool enable);
    /// Set enabled status recursively in the child hierarchy.
    void SetEnabledRecursive(bool enable);
    /// Set temporary mode. Temporary scene nodes are not saved.
    void SetTemporary(bool enable);
    /// Reparent the node.
    void SetParent(Node* newParent);
    /// Create a child node of specified type. A registered object factory for the type is required.
    Node* CreateChild(StringHash childType);
    /// Create a named child node of specified type. A registered object factory for the type is required.
    Node* CreateChild(StringHash childType, const String& childName);
    /// Add a node as a child. This accomplishes the same as SetParent, and can also be used for adding an externally created child node.
    void AddChild(Node* child);
    /// Destroy a child node.
    void DestroyChild(Node* child);
    /// Destroy a child node by index.
    void DestroyChild(size_t index);
    /// Destroy all child nodes.
    void DestroyAllChildren();
    /// Destroy self immediately. No operations on the node are valid after calling this.
    void DestroySelf();
    /// Create a child node of the specified type, template version.
    template <class T> T* CreateChild() { return static_cast<T*>(CreateChild(T::TypeStatic())); }
    /// Create a named child node of the specified type, template version.
    template <class T> T* CreateChild(const String& childName) { return static_cast<T*>(CreateChild(T::TypeStatic(), childName)); }
    
    /// Return name.
    const String& Name() const { return name; }
    /// Return enabled status.
    bool IsEnabled() const { return TestFlag(NF_ENABLED); }
    /// Return whether is temporary.
    bool IsTemporary() const { return TestFlag(NF_TEMPORARY); }
    /// Return parent node.
    Node* Parent() const { return parent; }
    /// Return the scene that the node belongs to.
    Scene* ParentScene() const { return scene; }
    /// Return unique id within the scene, or 0 if not in a scene.
    unsigned Id() const;
    /// Return number of immediate child nodes.
    size_t NumChildren() const { return children.Size(); }
    /// Return number of immediate child nodes that are not temporary.
    size_t NumPersistentChildren() const;
    /// Return an immediate child node by index.
    Node* Child(size_t index) const { return index < children.Size() ? children[index] : (Node*)0; }
    /// Return all child nodes.
    const Vector<Node*>& Children() const { return children; }
    /// Return child nodes recursively.
    void ChildrenRecursive(Vector<Node*>& dest);
    /// Find an immediate child node by name.
    Node* FindChild(const String& childName) const;
    /// Find the first immediate child node of specified type.
    Node* FindChild(StringHash childType) const;
    /// Find an immediate child node by type and name.
    Node* FindChild(StringHash childType, const String& childName) const;
    /// Find child node by name recursively.
    Node* FindChildRecursive(const String& childName) const;
    /// Find child node by type recursively.
    Node* FindChildRecursive(StringHash childType) const;
    /// Find child node by type and name recursively.
    Node* FindChildRecursive(StringHash childType, const String& childName) const;
    /// Find immediate child node by type, template version.
    template <class T> T* FindChild() const { return static_cast<T*>(FindChild(T::TypeStatic())); }
    /// Find immediate child node by type and name, template version.
    template <class T> T* FindChild(const String& childName) const { return static_cast<T*>(FindChild(T::TypeStatic(), childName)); }
    /// Find child node by type recursively, template version.
    template <class T> T* FindChildRecursive() const { return static_cast<T*>(FindChildRecursive(T::TypeStatic())); }
    /// Find child node by type and name recursively, template version.
    template <class T> T* FindChildRecursive(const String& childName) const { return static_cast<T*>(FindChildRecursive(T::TypeStatic(), childName)); }
    
    /// Set a bit flag. Called internally.
    void SetFlag(unsigned bit, bool set) const { if (set) flags |= bit; else flags &= ~bit; }
    /// Test a bit flag. Called internally.
    bool TestFlag(unsigned bit) const { return (flags & bit) != 0; }
    /// Assign node to a new scene. Called internally.
    void SetScene(Scene* newScene);
    
protected:
    /// Perform subclass-specific operation when assigned to a new parent node.
    virtual void OnParentSet(Node* newParent, Node* oldParent);
    /// Perform subclass-specific operation when assigned to a new scene.
    virtual void OnSceneSet(Scene* newScene, Scene* oldScene);
    /// Perform subclass-specific operation when the enabled status is changed.
    virtual void OnSetEnabled(bool newEnabled);

private:
    /// Node flags. Used to hold several boolean values (some subclass-specific) to reduce memory use.
    mutable unsigned flags;
    /// Parent node.
    Node* parent;
    /// Parent scene.
    Scene* scene;
    /// Child nodes. A node owns its children and destroys them during its own destruction.
    Vector<Node*> children;
    /// Node name.
    String name;
};

}
