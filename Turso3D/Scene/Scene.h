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

    /// Load whole scene from a binary stream. Existing nodes will be destroyed. Return true on success.
    bool Load(Deserializer& source);
    /// Load whole scene from JSON data. Existing nodes will be destroyed. Return true on success.
    bool LoadJSON(const JSONValue& source);
    /// Load JSON data as text from a binary stream, then load whole scene from it. Existing nodes will be destroyed. Return true if the JSON was correctly parsed; otherwise the data may be partial.
    bool LoadJSON(Deserializer& source);
    /// Instantiate node(s) from binary stream and return the root node.
    Node* Instantiate(Deserializer& source);
    /// Instantiate node(s) from JSON data and return the root node.
    Node* InstantiateJSON(const JSONValue& source);
    /// Load JSON data as text from a binary stream, then instantiate node(s) from it and return the root node.
    Node* InstantiateJSON(Deserializer& source);
    /// Destroy child nodes recursively, leaving the scene empty.
    void Clear();

    /// Find node by id.
    Node* FindNode(unsigned id) const;
    /// Find node's id by the node pointer.
    unsigned FindNodeId(Node* node) const;

    /// Add node to the scene. This assigns a scene-unique id to it.
    void AddNode(Node* node);
    /// Remove node from the scene. This removes the id mapping but does not destroy the node.
    void RemoveNode(Node* node);

private:
    /// Map from id's to nodes.
    HashMap<unsigned, Node*> nodes;
    /// Map from nodes to id's.
    HashMap<Node*, unsigned> ids;
    /// Next free node id.
    unsigned nextNodeId;
};

/// Register Scene related object factories and attributes.
TURSO3D_API void RegisterSceneLibrary();

}

