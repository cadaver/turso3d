// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Node.h"

/// %Scene root node, which also represents the whole scene.
class Scene : public Node
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
    /// Destroy child nodes recursively, leaving the scene empty.
    void Clear();

    /// Find node by id.
    Node* FindNode(unsigned id) const;

    /// Add node to the scene. This assigns a scene-unique id to it. Called internally.
    void AddNode(Node* node);
    /// Remove node from the scene. This removes the id mapping but does not destroy the node. Called internally.
    void RemoveNode(Node* node);
    
    using Node::Load;
    using Node::LoadJSON;
    using Node::SaveJSON;

private:
    /// Map from id's to nodes.
    std::map<unsigned, Node*> nodes;
    /// Next free node id.
    unsigned nextNodeId;
};

/// Register Scene related object factories and attributes.
void RegisterSceneLibrary();
