// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Node.h"

namespace Turso3D
{

/// %Scene root node, which also represents the whole scene.
class TURSO3D_API Scene : public Node
{
    OBJECT(Scene);

public:
    /// Construct.
    Scene();
    /// Destruct. The whole node tree is destroyed.
    ~Scene();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Save scene to binary stream.
    void Save(Stream& dest) override;
    
    /// Load scene from a binary stream. Existing nodes will be destroyed. Return true on success.
    bool Load(Stream& source);
    /// Load scene from JSON data. Existing nodes will be destroyed. Return true on success.
    bool LoadJSON(const JSONValue& source);
    /// Load scene from JSON text data read from a binary stream. Existing nodes will be destroyed. Return true if the JSON was correctly parsed; otherwise the data may be partial.
    bool LoadJSON(Stream& source);
    /// Save scene as JSON text data to a binary stream. Return true on success.
    bool SaveJSON(Stream& dest);
    /// Instantiate node(s) from binary stream and return the root node.
    Node* Instantiate(Stream& source);
    /// Instantiate node(s) from JSON data and return the root node.
    Node* InstantiateJSON(const JSONValue& source);
    /// Load JSON data as text from a binary stream, then instantiate node(s) from it and return the root node.
    Node* InstantiateJSON(Stream& source);
    /// Define a layer name. There can be 32 different layers (indices 0-31.)
    void DefineLayer(unsigned char index, const String& name);
    /// Define a tag name.
    void DefineTag(unsigned char index, const String& name);
    /// Destroy child nodes recursively, leaving the scene empty.
    void Clear();

    /// Find node by id.
    Node* FindNode(unsigned id) const;
    /// Return the layer names.
    const Vector<String>& LayerNames() const { return layerNames; }
    /// Return the layer name-to-index map.
    const HashMap<String, unsigned char>& Layers() const { return layers; }
    /// Return the tag names.
    const Vector<String>& TagNames() const { return tagNames; }
    /// Return the tag name-to-index map.
    const HashMap<String, unsigned char>& Tags() const { return tags; }

    /// Add node to the scene. This assigns a scene-unique id to it. Called internally.
    void AddNode(Node* node);
    /// Remove node from the scene. This removes the id mapping but does not destroy the node. Called internally.
    void RemoveNode(Node* node);
    
    using Node::Load;
    using Node::LoadJSON;
    using Node::SaveJSON;

private:
    /// Set layer names. Used in serialization.
    void SetLayerNamesAttr(JSONValue names);
    /// Return layer names. Used in serialization.
    JSONValue LayerNamesAttr() const;
    /// Set tag names. Used in serialization.
    void SetTagNamesAttr(JSONValue names);
    /// Return tag names. Used in serialization.
    JSONValue TagNamesAttr() const;

    /// Map from id's to nodes.
    HashMap<unsigned, Node*> nodes;
    /// Next free node id.
    unsigned nextNodeId;
    /// List of layer names by index.
    Vector<String> layerNames;
    /// Map from layer names to indices.
    HashMap<String, unsigned char> layers;
    /// List of tag names by index.
    Vector<String> tagNames;
    /// Map from tag names to indices.
    HashMap<String, unsigned char> tags;
};

/// Register Scene related object factories and attributes.
TURSO3D_API void RegisterSceneLibrary();

}

