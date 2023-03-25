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
struct ReinsertDrawablesTask;

/// Structure for raycast query results.
struct RaycastResult
{
    /// Hit world position.
    Vector3 position;
    /// Hit world normal.
    Vector3 normal;
    /// Hit distance along the ray.
    float distance;
    /// Hit drawable.
    Drawable* drawable;
    /// Hit node.
    OctreeNode* node;
    /// Hit geometry index or other, subclass-specific subobject index.
    size_t subObject;
};

/// %Octree cell, contains up to 8 child octants.
struct Octant
{
    /// Initialize parent and bounds.
    void Initialize(Octant* parent, const BoundingBox& boundingBox, unsigned char level);
    /// Test if a drawable should be inserted in this octant or if a smaller child octant should be created.
    bool FitBoundingBox(const BoundingBox& box, const Vector3& boxSize) const;
    /// Return child octant index based on position.
    size_t ChildIndex(const Vector3& position) const { size_t ret = position.x < center.x ? 0 : 1; ret += position.y < center.y ? 0 : 2; ret += position.z < center.z ? 0 : 4; return ret; }
    
    /// Drawables contained in the octant.
    std::vector<Drawable*> drawables;
    /// Expanded (loose) bounding box used for culling the octant and the drawables within it.
    BoundingBox cullingBox;
    /// Actual bounding box of the octant.
    BoundingBox worldBoundingBox;
    /// Bounding box center.
    Vector3 center;
    /// Bounding box half size.
    Vector3 halfSize;
    /// Subdivision level.
    unsigned char level;
    /// Drawable sort order dirty.
    bool sortDirty;
    /// Child octants.
    Octant* children[NUM_OCTANTS];
    /// Parent octant.
    Octant* parent;
    /// Number of drawables in this octant and the child octants combined.
    size_t numDrawables;
};

/// Acceleration structure for rendering. Should be created as a child of the scene root.
class Octree : public Node
{
    OBJECT(Octree);

public:
    /// Construct. The WorkQueue subsystem must have been initialized, as it will be used during update.
    Octree();
    /// Destruct. Delete all child octants and detach the drawables.
    ~Octree();

    /// Register factory and attributes.
    static void RegisterObject();
    
    /// Process the queue of nodes to be reinserted. Then sort nodes inside changed octants. This will utilize worker threads.
    void Update(unsigned short frameNumber);
    /// Resize the octree.
    void Resize(const BoundingBox& boundingBox, int numLevels);
    /// Enable or disable threaded update mode. In threaded mode reinsertions go to per-thread queues.
    void SetThreadedUpdate(bool enable) { threadedUpdate = enable; }
    /// Queue octree reinsertion for a drawable.
    void QueueUpdate(Drawable* drawable);
    /// Remove a drawable from the octree.
    void RemoveDrawable(Drawable* drawable);

    /// Query for drawables with a raycast and return all results.
    void Raycast(std::vector<RaycastResult>& result, const Ray& ray, unsigned short nodeFlags, float maxDistance = M_INFINITY, unsigned layerMask = LAYERMASK_ALL) const;
    /// Query for drawables with a raycast and return the closest result.
    RaycastResult RaycastSingle(const Ray& ray, unsigned short drawableFlags, float maxDistance = M_INFINITY, unsigned layerMask = LAYERMASK_ALL) const;
    /// Query for drawables using a volume such as frustum or sphere.
    template <class T> void FindDrawables(std::vector<Drawable*>& result, const T& volume, unsigned short drawableFlags, unsigned layerMask = LAYERMASK_ALL) const { CollectDrawables(result, const_cast<Octant*>(&root), volume, drawableFlags, layerMask); }
    /// Query for drawables using a frustum and masked testing.
    void FindDrawablesMasked(std::vector<Drawable*>& result, const Frustum& frustum, unsigned short drawableFlags, unsigned layerMask = LAYERMASK_ALL) const;

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
    /// Process a list of drawables to be reinserted. Clear the list afterward.
    void ReinsertDrawables(std::vector<Drawable*>& drawables);
    /// Remove a drawable from a reinsert queue.
    void RemoveDrawableFromQueue(Drawable* drawable, std::vector<Drawable*>& drawables);
    
    /// Add drawable to a specific octant.
    void AddDrawable(Drawable* drawable, Octant* octant)
    {
        octant->drawables.push_back(drawable);
        drawable->octant = octant;

        if (!octant->sortDirty)
        {
            octant->sortDirty = true;
            sortDirtyOctants.push_back(octant);
        }

        // Increment the drawable count in the whole parent branch
        while (octant)
        {
            ++octant->numDrawables;
            octant = octant->parent;
        }
    }

    /// Remove drawable rom an octant.
    void RemoveDrawable(Drawable* drawable, Octant* octant)
    {
        if (!octant)
            return;

        // Do not set the drawable's octant pointer to zero, as the drawable may already be added into another octant. Just remove from octant
        for (auto it = octant->drawables.begin(); it != octant->drawables.end(); ++it)
        {
            if ((*it) == drawable)
            {
                octant->drawables.erase(it);
                // Decrement the drawable count in the whole parent branch and erase empty octants as necessary
                while (octant)
                {
                    --octant->numDrawables;
                    Octant* next = octant->parent;
                    if (!octant->numDrawables && next)
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
    /// Get all drawables from an octant recursively.
    void CollectDrawables(std::vector<Drawable*>& result, Octant* octant) const;
    /// Get all drawables matching flags from an octant recursively.
    void CollectDrawables(std::vector<Drawable*>& result, Octant* octant, unsigned short drawableFlags, unsigned layerMask) const;
    /// Get all drawables matching flags along a ray.
    void CollectDrawables(std::vector<RaycastResult>& result, Octant* octant, const Ray& ray, unsigned short drawableFlags, float maxDistance, unsigned layerMask) const;
    /// Get all visible nodes matching flags that could be potential raycast hits.
    void CollectDrawables(std::vector<std::pair<Drawable*, float> >& result, Octant* octant, const Ray& ray, unsigned short drawableFlags, float maxDistance, unsigned layerMask) const;
    /// Work function to check reinsertion of nodes.
    void CheckReinsertWork(Task* task, unsigned threadIndex);

    /// Collect nodes matching flags using a volume such as frustum or sphere.
    template <class T> void CollectDrawables(std::vector<Drawable*>& result, Octant* octant, const T& volume, unsigned short drawableFlags, unsigned layerMask) const
    {
        Intersection res = volume.IsInside(octant->cullingBox);
        if (res == OUTSIDE)
            return;
        
        // If this octant is completely inside the volume, can include all contained octants and their nodes without further tests
        if (res == INSIDE)
            CollectDrawables(result, octant, drawableFlags, layerMask);
        else
        {
            std::vector<Drawable*>& drawables = octant->drawables;

            for (auto it = drawables.begin(); it != drawables.end(); ++it)
            {
                Drawable* drawable = *it;
                if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->LayerMask() & layerMask) && volume.IsInsideFast(drawable->WorldBoundingBox()) != OUTSIDE)
                    result.push_back(drawable);
            }
            
            for (size_t i = 0; i < NUM_OCTANTS; ++i)
            {
                if (octant->children[i])
                    CollectDrawables(result, octant->children[i], volume, drawableFlags, layerMask);
            }
        }
    }

    /// Collect nodes using a frustum and masked testing.
    void CollectDrawablesMasked(std::vector<Drawable*>& result, Octant* octant, const Frustum& frustum, unsigned short drawableFlags, unsigned layerMask, unsigned char planeMask = 0x3f) const
    {
        if (planeMask)
        {
            planeMask = frustum.IsInsideMasked(octant->cullingBox, planeMask);
            if (planeMask == 0xff)
                return;
        }

        std::vector<Drawable*>& drawables = octant->drawables;
     
        for (auto it = drawables.begin(); it != drawables.end(); ++it)
        {
            Drawable* drawable = *it;
            if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->LayerMask() & layerMask) && (!planeMask || frustum.IsInsideMaskedFast(drawable->WorldBoundingBox(), planeMask) != OUTSIDE))
                result.push_back(drawable);
        }

        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectDrawablesMasked(result, octant->children[i], frustum, drawableFlags, layerMask, planeMask);
        }
    }

    /// Threaded update flag. During threaded update moved OctreeNodes should go directly to thread-specific reinsert queues.
    volatile bool threadedUpdate;
    /// Current framenumber.
    unsigned short frameNumber;
    /// Queue of nodes to be reinserted.
    std::vector<Drawable*> updateQueue;
    /// Octants which need to have their drawables sorted.
    std::vector<Octant*> sortDirtyOctants;
    /// Root octant.
    Octant root;
    /// Allocator for child octants.
    Allocator<Octant> allocator;
    /// Cached %WorkQueue subsystem.
    WorkQueue* workQueue;
    /// Tasks for threaded reinsert execution.
    std::vector<AutoPtr<ReinsertDrawablesTask> > reinsertTasks;
    /// Intermediate reinsert queues for threaded execution.
    std::vector<std::vector<Drawable*> > reinsertQueues;
    /// RaycastSingle initial coarse result.
    mutable std::vector<std::pair<Drawable*, float> > initialRayResult;
    /// RaycastSingle final result.
    mutable std::vector<RaycastResult> finalRayResult;
};

/// Task for octree drawables reinsertion.
struct ReinsertDrawablesTask : public MemberFunctionTask<Octree>
{
    /// Construct.
    ReinsertDrawablesTask(Octree* object_, MemberWorkFunctionPtr function_) :
        MemberFunctionTask<Octree>(object_, function_)
    {
    }

    /// Start pointer.
    Drawable** start;
    /// End pointer.
    Drawable** end;
};
