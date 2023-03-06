// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Frustum.h"
#include "../Object/Allocator.h"
#include "../Thread/WorkQueue.h"
#include "OctreeNode.h"

static const size_t NUM_OCTANTS = 8;

class Octree;
class OctreeNode;
class Ray;
class WorkQueue;
struct Task;

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
    /// Initialize parent and bounds.
    void Initialize(Octant* parent, const BoundingBox& boundingBox, unsigned char level);
    /// Test if a node should be inserted in this octant or if a smaller child octant should be created.
    bool FitBoundingBox(const BoundingBox& box, const Vector3& boxSize) const;
    /// Return child octant index based on position.
    size_t ChildIndex(const Vector3& position) const { size_t ret = position.x < center.x ? 0 : 1; ret += position.y < center.y ? 0 : 2; ret += position.z < center.z ? 0 : 4; return ret; }
    
    /// Nodes contained in the octant.
    std::vector<OctreeNode*> nodes;
    /// Expanded (loose) bounding box used for culling the octant and the nodes within it.
    BoundingBox cullingBox;
    /// Actual bounding box of the octant.
    BoundingBox worldBoundingBox;
    /// Bounding box center.
    Vector3 center;
    /// Bounding box half size.
    Vector3 halfSize;
    /// Subdivision level.
    unsigned char level;
    /// Node sorting dirty.
    bool sortDirty;
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
    /// Construct. The WorkQueue subsystem must have been initialized, as it will be used during update.
    Octree();
    /// Destruct. Delete all child octants and detach the nodes.
    ~Octree();

    /// Register factory and attributes.
    static void RegisterObject();
    
    /// Process the queue of nodes to be reinserted. Then sort nodes inside changed octants. This will utilize worker threads.
    void Update(unsigned short frameNumber);
    /// Resize octree.
    void Resize(const BoundingBox& boundingBox, int numLevels);

    /// Queue octree reinsertion for a node.
    void QueueUpdate(OctreeNode* node)
    {
        assert(node);

        if (!threadedUpdate)
        {
            updateQueue.push_back(node);
            node->SetFlag(NF_OCTREE_REINSERT_QUEUED, true);
        }
        else
        {
            node->lastUpdateFrameNumber = frameNumber;

            // Do nothing if still fits the current octant
            const BoundingBox& box = node->WorldBoundingBox();
            Octant* oldOctant = node->octant;
            if (!oldOctant || oldOctant->cullingBox.IsInside(box) != INSIDE)
            {
                reinsertQueues[WorkQueue::ThreadIndex()].push_back(node);
                node->SetFlag(NF_OCTREE_REINSERT_QUEUED, true);
            }
        }
    }

    /// Remove a node from the octree.
    void RemoveNode(OctreeNode* node);
    /// Enable or disable threaded update mode. In threaded mode reinsertions go to per-thread queues.
    void SetThreadedUpdate(bool enable);

    /// Query for nodes with a raycast and return all results.
    void Raycast(std::vector<RaycastResult>& result, const Ray& ray, unsigned short nodeFlags, float maxDistance = M_INFINITY, unsigned layerMask = LAYERMASK_ALL) const;
    /// Query for nodes with a raycast and return the closest result.
    RaycastResult RaycastSingle(const Ray& ray, unsigned short nodeFlags, float maxDistance = M_INFINITY, unsigned layerMask = LAYERMASK_ALL) const;

    /// Query for nodes using a volume such as frustum or sphere.
    template <class T> void FindNodes(std::vector<OctreeNode*>& result, const T& volume, unsigned short nodeFlags, unsigned layerMask = LAYERMASK_ALL) const
    {
        CollectNodes(result, const_cast<Octant*>(&root), volume, nodeFlags, layerMask);
    }

    /// Collect nodes matching flags using a frustum and masked testing.
    void FindNodesMasked(std::vector<OctreeNode*>& result, const Frustum& frustum, unsigned short nodeFlags, unsigned layerMask = LAYERMASK_ALL) const
    {
        CollectNodesMasked(result, const_cast<Octant*>(&root), frustum, nodeFlags, layerMask);
    }

    /// Return whether threaded update is enabled.
    bool ThreadedUpdate() const { return threadedUpdate; }
    /// Return the root octant.
    Octant* Root() const { return const_cast<Octant*>(&root); }

private:
    /// Set bounding box. Used in serialization.
    void SetBoundingBoxAttr(const BoundingBox& value);
    /// Return bounding box. Used in serialization.
    const BoundingBox& BoundingBoxAttr() const;
    /// Set number of levels. Used in serialization.
    void SetNumLevelsAttr(int numLevels);
    /// Return number of levels. Used in serialization.
    int NumLevelsAttr() const;
    /// Process a list of nodes to be reinserted. Clear the list afterward.
    void ReinsertNodes(std::vector<OctreeNode*>& nodes);
    /// Remove a node from a reinsert queue.
    void RemoveNodeFromQueue(OctreeNode* node, std::vector<OctreeNode*>& nodes);
    
    /// Add node to a specific octant.
    void AddNode(OctreeNode* node, Octant* octant)
    {
        octant->nodes.push_back(node);
        node->octant = octant;

        if (!octant->sortDirty)
        {
            octant->sortDirty = true;
            sortDirtyOctants.push_back(octant);
        }

        // Increment the node count in the whole parent branch
        while (octant)
        {
            ++octant->numNodes;
            octant = octant->parent;
        }
    }

    /// Remove node from an octant.
    void RemoveNode(OctreeNode* node, Octant* octant)
    {
        if (!octant)
            return;

        // Do not set the node's octant pointer to zero, as the node may already be added into another octant. Just remove from octant
        for (auto it = octant->nodes.begin(); it != octant->nodes.end(); ++it)
        {
            if ((*it) == node)
            {
                octant->nodes.erase(it);
                // Decrement the node count in the whole parent branch and erase empty octants as necessary
                while (octant)
                {
                    --octant->numNodes;
                    Octant* next = octant->parent;
                    if (!octant->numNodes && next)
                        DeleteChildOctant(next, next->ChildIndex(octant->center));
                    octant = next;
                }
                return;
            }
        }
    }

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
    /// Work function to check reinsertion of nodes.
    void CheckReinsertWork(Task* task, unsigned threadIndex);

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

    /// Collect nodes using a frustum and masked testing.
    void CollectNodesMasked(std::vector<OctreeNode*>& result, Octant* octant, const Frustum& frustum, unsigned short nodeFlags, unsigned layerMask, unsigned char planeMask = 0x3f) const
    {
        if (planeMask)
        {
            planeMask = frustum.IsInsideMasked(octant->cullingBox, planeMask);
            if (planeMask == 0xff)
                return;
        }

        std::vector<OctreeNode*>& octantNodes = octant->nodes;
     
        for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
        {
            OctreeNode* node = *it;
            if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask) && (!planeMask || frustum.IsInsideMaskedFast(node->WorldBoundingBox(), planeMask) != OUTSIDE))
                result.push_back(node);
        }

        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectNodesMasked(result, octant->children[i], frustum, nodeFlags, layerMask, planeMask);
        }
    }

    /// Threaded update flag. During threaded update moved OctreeNodes should go directly to thread-specific reinsert queues.
    volatile bool threadedUpdate;
    /// Current framenumber.
    unsigned short frameNumber;
    /// Queue of nodes to be reinserted.
    std::vector<OctreeNode*> updateQueue;
    /// Octants which need to have sort order updated.
    std::vector<Octant*> sortDirtyOctants;
    /// Root octant.
    Octant root;
    /// Allocator for child octants.
    Allocator<Octant> allocator;
    /// Cached %WorkQueue subsystem.
    WorkQueue* workQueue;
    /// Tasks for threaded reinsert execution.
    std::vector<AutoPtr<Task> > reinsertTasks;
    /// Intermediate reinsert queues for threaded execution.
    std::vector<std::vector<OctreeNode*> > reinsertQueues;
    /// RaycastSingle initial coarse result.
    mutable std::vector<std::pair<OctreeNode*, float> > initialRayResult;
    /// RaycastSingle final result.
    mutable std::vector<RaycastResult> finalRayResult;
};
