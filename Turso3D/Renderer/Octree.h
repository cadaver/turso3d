// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Frustum.h"
#include "../Time/Profiler.h"
#include "../Object/Allocator.h"
#include "OctreeNode.h"

#include <algorithm>

static const size_t NUM_OCTANTS = 8;

class Octree;
class OctreeNode;
class Ray;

/// Structure for raycast query results.
struct RaycastResult
{
    /// Hit world position.
    Vector3 position;
    /// Hit world normal.
    Vector3 normal;
    /// Hit distance along the ray.
    float distance;
    /// Hit node.
    OctreeNode* node;
    /// Hit geometry index or other, GeometryNode subclass-specific subobject index.
    size_t subObject;
};

/// %Octree cell, contains up to 8 child octants.
struct Octant
{
    /// Construct.
    Octant();
   
    /// Initialize parent and bounds.
    void Initialize(Octant* parent, const BoundingBox& boundingBox, int level);
    /// Test if a node should be inserted in this octant or if a smaller child octant should be created.
    bool FitBoundingBox(const BoundingBox& box, const Vector3& boxSize) const;
    /// Return child octant index based on position.
    size_t ChildIndex(const Vector3& position) const { size_t ret = position.x < center.x ? 0 : 1; ret += position.y < center.y ? 0 : 2; ret += position.z < center.z ? 0 : 4; return ret; }
    
    /// Nodes contained in the octant.
    std::vector<OctreeNode*> nodes;
    /// Node sorting dirty.
    bool sortDirty;
    /// Expanded (loose) bounding box used for culling the octant and the nodes within it.
    BoundingBox cullingBox;
    /// Actual bounding box of the octant.
    BoundingBox worldBoundingBox;
    /// Bounding box center.
    Vector3 center;
    /// Bounding box half size.
    Vector3 halfSize;
    /// Subdivision level.
    int level;
    /// Child octants.
    Octant* children[NUM_OCTANTS];
    /// Parent octant.
    Octant* parent;
    /// Number of nodes in this octant and the child octants combined.
    size_t numNodes;
};

/// Acceleration structure for rendering. Should be created as a child of the scene root.
class Octree : public Node
{
    OBJECT(Octree);

public:
    /// Construct.
    Octree();
    /// Destruct. Delete all child octants and detach the nodes.
    ~Octree();

    /// Register factory and attributes.
    static void RegisterObject();
    
    /// Process the queue of nodes to be reinserted. Then sort nodes inside changed octants.
    void Update(unsigned short frameNumber);
    /// Resize octree.
    void Resize(const BoundingBox& boundingBox, int numLevels);
    /// Remove a node from the octree.
    void RemoveNode(OctreeNode* node);
    /// Queue a reinsertion for a node.
    void QueueUpdate(OctreeNode* node);
    /// Cancel a pending reinsertion.
    void CancelUpdate(OctreeNode* node);
    /// Query for nodes with a raycast and return all results.
    void Raycast(std::vector<RaycastResult>& result, const Ray& ray, unsigned short nodeFlags, float maxDistance = M_INFINITY, unsigned layerMask = LAYERMASK_ALL);
    /// Query for nodes with a raycast and return the closest result.
    RaycastResult RaycastSingle(const Ray& ray, unsigned short nodeFlags, float maxDistance = M_INFINITY, unsigned layerMask = LAYERMASK_ALL);

    /// Query for nodes using a volume such as frustum or sphere.
    template <class T> void FindNodes(std::vector<OctreeNode*>& result, const T& volume, unsigned short nodeFlags, unsigned layerMask = LAYERMASK_ALL)
    {
        CollectNodes(result, &root, volume, nodeFlags, layerMask);
    }

    /// Query for nodes using a volume such as frustum or sphere. Invoke a member function for each octant.
    template <class T, class U> void FindNodes(const T& volume, U* object, void (U::*callback)(std::vector<OctreeNode*>::const_iterator, std::vector<OctreeNode*>::const_iterator, bool))
    {
        CollectNodesMemberCallback(&root, volume, object, callback);
    }

    /// Collect nodes matching flags using a frustum and masked testing.
    void FindNodesMasked(std::vector<OctreeNode*>& result, const Frustum& frustum, unsigned short nodeFlags, unsigned layerMask = LAYERMASK_ALL)
    {
        CollectNodesMasked(result, &root, frustum, nodeFlags, layerMask);
    }

    /// Collect nodes matching flags using a frustum and masked testing. Invoke a member callback for each octant, with current mask provided.
    template <class T> void FindNodesMasked(const Frustum& frustum, T* object, void (T::*callback)(std::vector<OctreeNode*>::const_iterator, std::vector<OctreeNode*>::const_iterator, unsigned char))
    {
        CollectNodesMaskedMemberCallback(&root, frustum, object, callback);
    }

private:
    /// Set bounding box. Used in serialization.
    void SetBoundingBoxAttr(const BoundingBox& boundingBox);
    /// Return bounding box. Used in serialization.
    const BoundingBox& BoundingBoxAttr() const;
    /// Set number of levels. Used in serialization.
    void SetNumLevelsAttr(int numLevels);
    /// Return number of levels. Used in serialization.
    int NumLevelsAttr() const;
    /// Add node to a specific octant.
    void AddNode(OctreeNode* node, Octant* octant);
    /// Remove node from an octant.
    void RemoveNode(OctreeNode* node, Octant* octant);
    /// Create a new child octant.
    Octant* CreateChildOctant(Octant* octant, size_t index);
    /// Delete one child octant.
    void DeleteChildOctant(Octant* octant, size_t index);
    /// Delete a child octant hierarchy. If not deleting the octree for good, moves any nodes back to the root octant.
    void DeleteChildOctants(Octant* octant, bool deletingOctree);
    /// Get all nodes from an octant recursively.
    void CollectNodes(std::vector<OctreeNode*>& result, Octant* octant) const;
    /// Get all visible nodes matching flags from an octant recursively.
    void CollectNodes(std::vector<OctreeNode*>& result, Octant* octant, unsigned short nodeFlags, unsigned layerMask) const;
    /// Get all visible nodes matching flags along a ray.
    void CollectNodes(std::vector<RaycastResult>& result, Octant* octant, const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask) const;
    /// Get all visible nodes matching flags that could be potential raycast hits.
    void CollectNodes(std::vector<std::pair<OctreeNode*, float> >& result, Octant* octant, const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask) const;

    /// Collect nodes matching flags using a volume such as frustum or sphere.
    template <class T> void CollectNodes(std::vector<OctreeNode*>& result, Octant* octant, const T& volume, unsigned short nodeFlags, unsigned layerMask) const
    {
        Intersection res = volume.IsInside(octant->cullingBox);
        if (res == OUTSIDE)
            return;
        
        // If this octant is completely inside the volume, can include all contained octants and their nodes without further tests
        if (res == INSIDE)
            CollectNodes(result, octant, nodeFlags, layerMask);
        else
        {
            std::vector<OctreeNode*>& octantNodes = octant->nodes;

            for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
            {
                OctreeNode* node = *it;
                if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask) &&
                    volume.IsInsideFast(node->WorldBoundingBox()) != OUTSIDE)
                    result.push_back(node);
            }
            
            for (size_t i = 0; i < NUM_OCTANTS; ++i)
            {
                if (octant->children[i])
                    CollectNodes(result, octant->children[i], volume, nodeFlags, layerMask);
            }
        }
    }

    /// Collect nodes from octant and child octants. Invoke a member function for each octant.
    template <class T> void CollectNodesMemberCallback(Octant* octant, T* object, void (T::*callback)(std::vector<OctreeNode*>::const_iterator, std::vector<OctreeNode*>::const_iterator, bool)) const
    {
        std::vector<OctreeNode*>& octantNodes = octant->nodes;

        if (octantNodes.size())
            (object->*callback)(octantNodes.begin(), octantNodes.end(), true);

        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectNodesMemberCallback(octant->children[i], object, callback);
        }
    }

    /// Collect nodes using a volume such as frustum or sphere. Invoke a member function for each octant.
    template <class T, class U> void CollectNodesMemberCallback(Octant* octant, const T& volume, U* object, void (U::*callback)(std::vector<OctreeNode*>::const_iterator, std::vector<OctreeNode*>::const_iterator, bool)) const
    {
        Intersection res = volume.IsInside(octant->cullingBox);
        if (res == OUTSIDE)
            return;

        // If this octant is completely inside the volume, can include all contained octants and their nodes without further tests
        if (res == INSIDE)
            CollectNodesMemberCallback(octant, object, callback);
        else
        {
            std::vector<OctreeNode*>& octantNodes = octant->nodes;

            if (octantNodes.size())
                (object->*callback)(octantNodes.begin(), octantNodes.end(), false);

            for (size_t i = 0; i < NUM_OCTANTS; ++i)
            {
                if (octant->children[i])
                    CollectNodesMemberCallback(octant->children[i], volume, object, callback);
            }
        }
    }

    /// Collect nodes using a frustum and masked testing.
    void CollectNodesMasked(std::vector<OctreeNode*>& result, Octant* octant, const Frustum& frustum, unsigned short nodeFlags, unsigned layerMask, unsigned char planeMask = 0) const
    {
        if (planeMask != 0x3f)
        {
            planeMask = frustum.IsInsideMasked(octant->cullingBox, planeMask);
            if (planeMask == 0xff)
                return;
        }

        std::vector<OctreeNode*>& octantNodes = octant->nodes;
     
        for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
        {
            OctreeNode* node = *it;
            if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask) && (planeMask == 0x3f || frustum.IsInsideMaskedFast(node->WorldBoundingBox(), planeMask) != OUTSIDE))
                result.push_back(node);
        }

        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectNodesMasked(result, octant->children[i], frustum, nodeFlags, layerMask, planeMask);
        }
    }

    /// Collect nodes using a frustum and masked testing. Invoke a member function for each octant.
    template <class T> void CollectNodesMaskedMemberCallback(Octant* octant, const Frustum& frustum, T* object, void (T::*callback)(std::vector<OctreeNode*>::const_iterator, 
        std::vector<OctreeNode*>::const_iterator, unsigned char), unsigned char planeMask = 0) const
    {
        if (planeMask != 0x3f)
        {
            planeMask = frustum.IsInsideMasked(octant->cullingBox, planeMask);
            if (planeMask == 0xff)
                return;
        }

        std::vector<OctreeNode*>& octantNodes = octant->nodes;

        if (octantNodes.size())
            (object->*callback)(octantNodes.begin(), octantNodes.end(), planeMask);

        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectNodesMaskedMemberCallback(octant->children[i], frustum, object, callback, planeMask);
        }
    }

    /// Queue of nodes to be reinserted.
    std::vector<OctreeNode*> updateQueue;
    /// Octants which need to have sort order updated.
    std::vector<Octant*> sortDirtyOctants;
    /// RaycastSingle initial coarse result.
    std::vector<std::pair<OctreeNode*, float> > initialRes;
    /// RaycastSingle final result.
    std::vector<RaycastResult> finalRes;
    /// Allocator for child octants.
    Allocator<Octant> allocator;
    /// Root octant.
    Octant root;
};
