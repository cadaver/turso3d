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
static const unsigned char TAG_NONE = 0x0;
static const unsigned LAYERMASK_ALL = 0xffffffff;

/// Base class for scene nodes.
class TURSO3D_API Node : public Serializable
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
    unsigned Id() const override { return id; }

    /// Save as JSON text data to a binary stream. Return true on success.
    bool SaveJSON(Stream& dest);
    /// Set name. Is not required to be unique within the scene.
    void SetName(const String& newName);
    /// Set name.
    void SetName(const char* newName);
    /// Set node's layer. Usage is subclass specific, for example rendering nodes selectively. Default is 0.
    void SetLayer(unsigned char newLayer);
    /// Set node's layer by name. The layer name must have been registered to the scene root beforehand.
    void SetLayerName(const String& newLayerName);
    /// Set node's tag, which can be used to search for specific nodes.
    void SetTag(unsigned char newTag);
    /// Set node's tag by name. The tag name must have been registered to the scene root beforehand.
    void SetTagName(const String& newTagName);
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
    /// Remove child node. Will delete it if there are no other strong references to it.
    void RemoveChild(Node* child);
    /// Remove child node by index.
    void RemoveChild(size_t index);
    /// Remove all child nodes.
    void RemoveAllChildren();
    /// Remove self immediately. As this will delete the node (if no other strong references) no operations on the node are valid after calling this.
    void RemoveSelf();
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
    /// Return layer name, or empty if not registered in the scene root.
    const String& LayerName() const;
    /// Return bitmask corresponding to layer.
    unsigned LayerMask() const { return 1 << layer; }
    /// Return tag.
    unsigned char Tag() const { return tag; }
    /// Return tag name, or empty if not registered in the scene root.
    const String& TagName() const;
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
    Node* Child(size_t index) const { return index < children.Size() ? children[index] : nullptr; }
    /// Return all immediate child nodes.
    const Vector<SharedPtr<Node> >& Children() const { return children; }
    /// Return child nodes recursively.
    void AllChildren(Vector<Node*>& result) const;
    /// Return first child node that matches name.
    Node* FindChild(const String& childName, bool recursive = false) const;
    /// Return first child node that matches name.
    Node* FindChild(const char* childName, bool recursive = false) const;
    /// Return first child node of specified type.
    Node* FindChild(StringHash childType, bool recursive = false) const;
    /// Return first child node that matches type and name.
    Node* FindChild(StringHash childType, const String& childName, bool recursive = false) const;
    /// Return first child node that matches type and name.
    Node* FindChild(StringHash childType, const char* childName, bool recursive = false) const;
    /// Return first child node that matches layer mask.
    Node* FindChildByLayer(unsigned layerMask, bool recursive = false) const;
    /// Return first child node that matches tag.
    Node* FindChildByTag(unsigned char tag, bool recursive = false) const;
    /// Return first child node that matches tag name.
    Node* FindChildByTag(const String& tagName, bool recursive = false) const;
    /// Return first child node that matches tag name.
    Node* FindChildByTag(const char* tagName, bool recursive = false) const;
    /// Find child nodes of specified type.
    void FindChildren(Vector<Node*>& result, StringHash childType, bool recursive = false) const;
    /// Find child nodes that match layer mask.
    void FindChildrenByLayer(Vector<Node*>& result, unsigned layerMask, bool recursive = false) const;
    /// Find child nodes that match tag.
    void FindChildrenByTag(Vector<Node*>& result, unsigned char tag, bool recursive = false) const;
    /// Find child nodes that match tag name.
    void FindChildrenByTag(Vector<Node*>& result, const String& tagName, bool recursive = false) const;
    /// Find child nodes that match tag name.
    void FindChildrenByTag(Vector<Node*>& result, const char* tagName, bool recursive = false) const;
    /// Return number of sibling nodes.
    size_t NumSiblings() const;
    /// Return sibling node by index.
    Node* Sibling(size_t index) const;
    /// Return all sibling nodes including self.
    const Vector<SharedPtr<Node> >& Siblings() const;
    /// Return first sibling node that matches name.
    Node* FindSibling(const String& siblingName) const;
    /// Return first sibling node that matches name.
    Node* FindSibling(const char* siblingName) const;
    /// Return first sibling node of specified type.
    Node* FindSibling(StringHash siblingType) const;
    /// Return first sibling node that matches type and name.
    Node* FindSibling(StringHash siblingType, const String& siblingName) const;
    /// Return first sibling node that matches type and name.
    Node* FindSibling(StringHash siblingType, const char* siblingName) const;
    /// Return first sibling node that matches layer mask.
    Node* FindSiblingByLayer(unsigned layerMask) const;
    /// Return first sibling node that matches tag.
    Node* FindSiblingByTag(unsigned char tag) const;
    /// Return first sibling node that matches tag name.
    Node* FindSiblingByTag(const String& tagName) const;
    /// Return first sibling node that matches tag name.
    Node* FindSiblingByTag(const char* tagName) const;
    /// Find sibling nodes of specified type.
    void FindSiblings(Vector<Node*>& result, StringHash siblingType) const;
    /// Find sibling nodes that match layer mask.
    void FindSiblingsByLayer(Vector<Node*>& result, unsigned layerMask) const;
    /// Find sibling nodes that match tag.
    void FindSiblingsByTag(Vector<Node*>& result, unsigned char tag) const;
    /// Find sibling nodes that match tag name.
    void FindSiblingsByTag(Vector<Node*>& result, const String& tagName) const;
    /// Find sibling nodes that match tag name.
    void FindSiblingsByTag(Vector<Node*>& result, const char* tagName) const;
    /// Return first child node of specified type, template version.
    template <class T> T* FindChild(bool recursive = false) const { return static_cast<T*>(FindChild(T::TypeStatic(), recursive)); }
    /// Return first child node that matches type and name, template version.
    template <class T> T* FindChild(const String& childName, bool recursive = false) const { return static_cast<T*>(FindChild(T::TypeStatic(), childName, recursive)); }
    /// Return first child node that matches type and name, template version.
    template <class T> T* FindChild(const char* childName, bool recursive = false) const { return static_cast<T*>(FindChild(T::TypeStatic(), childName, recursive)); }
    /// Find child nodes of specified type, template version.
    template <class T> void FindChildren(Vector<T*>& result, bool recursive = false) const { return FindChildren(reinterpret_cast<Vector<T*>&>(result), recursive); }
    /// Return first sibling node of specified type, template version.
    template <class T> T* FindSibling() const { return static_cast<T*>(FindSibling(T::TypeStatic())); }
    /// Return first sibling node that matches type and name, template version.
    template <class T> T* FindSibling(const String& siblingName) const { return static_cast<T*>(FindSibling(T::TypeStatic(), siblingName)); }
    /// Return first sibling node that matches type and name, template version.
    template <class T> T* FindSibling(const char* siblingName) const { return static_cast<T*>(FindSibling(T::TypeStatic(), siblingName)); }
    /// Find sibling nodes of specified type, template version.
    template <class T> void FindSiblings(Vector<T*>& result) const { return FindSiblings(reinterpret_cast<Vector<T*>&>(result)); }

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
    static void SkipHierarchy(Stream& source);

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
    /// Tag number.
    unsigned char tag;
    /// Parent node.
    Node* parent;
    /// Parent scene.
    Scene* scene;
    /// Child nodes.
    Vector<SharedPtr<Node> > children;
    /// Id within the scene.
    unsigned id;
    /// %Node name.
    String name;
};

}
