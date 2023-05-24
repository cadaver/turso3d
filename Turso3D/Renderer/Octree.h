// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Frustum.h"
#include "../Thread/WorkQueue.h"
#include "OctreeNode.h"

#include <atomic>

static const size_t NUM_OCTANTS = 8;
static const unsigned char OF_DRAWABLES_SORT_DIRTY = 0x1;
static const unsigned char OF_CULLING_BOX_DIRTY = 0x2;
static const float OCCLUSION_QUERY_INTERVAL = 0.133333f; // About 8 frame stagger at 60fps

class Ray;
class WorkQueue;
struct ReinsertDrawablesTask;

/// %Octant occlusion query visibility states.
enum OctantVisibility
{
    VIS_OUTSIDE_FRUSTUM = 0,
    VIS_OCCLUDED,
    VIS_OCCLUDED_UNKNOWN,
    VIS_VISIBLE_UNKNOWN,
    VIS_VISIBLE
};

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
    /// Hit geometry index or other, subclass-specific subobject index.
    size_t subObject;
};

/// %Octree cell, contains up to 8 child octants.
class Octant
{
    friend class Octree;

public:
    /// Construct with defaults.
    Octant();
    /// Destruct. If has a pending occlusion query, free it.
    ~Octant();

    /// Initialize parent and bounds.
    void Initialize(Octant* parent, const BoundingBox& boundingBox, unsigned char level, unsigned char childIndex);
    /// Add debug geometry to be rendered.
    void OnRenderDebug(DebugRenderer* debug);
    /// React to occlusion query being rendered for the octant. Store the query ID to know not to re-test until have the result.
    void OnOcclusionQuery(unsigned queryId);
    /// React to occlusion query result. Push changed visibility to parents or children as necessary. If outside frustum, no operation.
    void OnOcclusionQueryResult(bool visible);

    /// Return the culling box. Update as necessary.
    const BoundingBox& CullingBox() const;
    /// Return drawables in this octant.
    const std::vector<Drawable*>& Drawables() const { return drawables; }
    /// Return whether has child octants.
    bool HasChildren() const { return numChildren > 0; }
    /// Return child octant by index.
    Octant* Child(size_t index) const { return children[index]; }
    /// Return parent octant.
    Octant* Parent() const { return parent; }
    /// Return child octant index based on position.
    unsigned char ChildIndex(const Vector3& position) const { unsigned char ret = position.x < center.x ? 0 : 1; ret += position.y < center.y ? 0 : 2; ret += position.z < center.z ? 0 : 4; return ret; }
    /// Return last occlusion visibility status.
    OctantVisibility Visibility() const { return (OctantVisibility)visibility; }
    /// Return whether is pending an occlusion query result.
    bool OcclusionQueryPending() const { return occlusionQueryId != 0; }
    /// Set bit flag. Called internally.
    void SetFlag(unsigned char bit, bool set) const { if (set) flags |= bit; else flags &= ~bit; }
    /// Test bit flag. Called internally.
    bool TestFlag(unsigned char bit) const { return (flags & bit) != 0; }

    /// Test if a drawable should be inserted in this octant or if a smaller child octant should be created.
    bool FitBoundingBox(const BoundingBox& box, const Vector3& boxSize) const
    {
        // If max split level, size always OK, otherwise check that box is at least half size of octant
        if (level <= 1 || boxSize.x >= halfSize.x || boxSize.y >= halfSize.y || boxSize.z >= halfSize.z)
            return true;
        // Also check if the box can not fit inside a child octant's culling box, in that case size OK (must insert here)
        else
        {
            Vector3 quarterSize = 0.5f * halfSize;
            if (box.min.x <= fittingBox.min.x + quarterSize.x || box.max.x >= fittingBox.max.x - quarterSize.x ||
                box.min.y <= fittingBox.min.y + quarterSize.y || box.max.y >= fittingBox.max.y - quarterSize.y ||
                box.max.z <= fittingBox.min.z + quarterSize.z || box.max.z >= fittingBox.max.z - quarterSize.z)
                return true;
        }

        // Bounding box too small, should create a child octant
        return false;
    }

    /// Mark culling boxes dirty in the parent hierarchy.
    void MarkCullingBoxDirty() const
    {
        const Octant* octant = this;

        while (octant && !octant->TestFlag(OF_CULLING_BOX_DIRTY))
        {
            octant->SetFlag(OF_CULLING_BOX_DIRTY, true);
            octant = octant->parent;
        }
    }

    /// Push visibility status to child octants.
    void PushVisibilityToChildren(Octant* octant, OctantVisibility newVisibility)
    {
        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
            {
                octant->children[i]->visibility = newVisibility;
                if (octant->children[i]->numChildren)
                    PushVisibilityToChildren(octant->children[i], newVisibility);
            }
        }
    }

    /// Set visibility status manually.
    void SetVisibility(OctantVisibility newVisibility, bool pushToChildren = false)
    {
        visibility = newVisibility;

        if (pushToChildren)
            PushVisibilityToChildren(this, newVisibility);
    }

    /// Return true if a new occlusion query should be executed. Use a time interval for already visible octants. Return false if previous query still pending.
    bool CheckNewOcclusionQuery(float frameTime)
    {
        if (visibility != VIS_VISIBLE)
            return occlusionQueryId == 0;

        occlusionQueryTimer += frameTime;

        if (occlusionQueryId != 0)
            return false;

        if (occlusionQueryTimer >= OCCLUSION_QUERY_INTERVAL)
        {
            occlusionQueryTimer = fmodf(occlusionQueryTimer, OCCLUSION_QUERY_INTERVAL);
            return true;
        }
        else
            return false;
    }

private:
    /// Combined drawable and child octant bounding box. Used for culling tests.
    mutable BoundingBox cullingBox;
    /// Drawables contained in the octant.
    std::vector<Drawable*> drawables;
    /// Expanded (loose) bounding box used for fitting drawables within the octant.
    BoundingBox fittingBox;
    /// Bounding box center.
    Vector3 center;
    /// Bounding box half size.
    Vector3 halfSize;
    /// Child octants.
    Octant* children[NUM_OCTANTS];
    /// Parent octant.
    Octant* parent;
    /// Last occlusion query visibility.
    OctantVisibility visibility;
    /// Occlusion query id, or 0 if no query pending.
    unsigned occlusionQueryId;
    /// Occlusion query interval timer.
    float occlusionQueryTimer;
    /// Number of child octants.
    unsigned char numChildren;
    /// Subdivision level, decreasing for child octants.
    unsigned char level;
    /// The child index of this octant.
    unsigned char childIndex;
    /// Dirty flags.
    mutable unsigned char flags;
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
    
    /// Process the queue of nodes to be reinserted. This will utilize worker threads.
    void Update(unsigned short frameNumber);
    /// Finish the octree update.
    void FinishUpdate();
    /// Resize the octree.
    void Resize(const BoundingBox& boundingBox, int numLevels);
    /// Enable or disable threaded update mode. In threaded mode reinsertions go to per-thread queues, which are processed in FinishUpdate().
    void SetThreadedUpdate(bool enable) { threadedUpdate = enable; }
    /// Queue octree reinsertion for a drawable.
    void QueueUpdate(Drawable* drawable);
    /// Remove a drawable from the octree.
    void RemoveDrawable(Drawable* drawable);
    /// Add debug geometry to be rendered. Visualizes the whole octree.
    void OnRenderDebug(DebugRenderer* debug);

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
        octant->MarkCullingBoxDirty();
        drawable->octant = octant;

        if (!octant->TestFlag(OF_DRAWABLES_SORT_DIRTY))
        {
            octant->SetFlag(OF_DRAWABLES_SORT_DIRTY, true);
            sortDirtyOctants.push_back(octant);
        }
    }

    /// Remove drawable from an octant.
    void RemoveDrawable(Drawable* drawable, Octant* octant)
    {
        if (!octant)
            return;

        octant->MarkCullingBoxDirty();

        // Do not set the drawable's octant pointer to zero, as the drawable may already be added into another octant. Just remove from octant
        for (auto it = octant->drawables.begin(); it != octant->drawables.end(); ++it)
        {
            if ((*it) == drawable)
            {
                octant->drawables.erase(it);

                // Erase empty octants as necessary, but never the root
                while (!octant->drawables.size() && !octant->numChildren && octant->parent)
                {
                    DeleteChildOctant(octant->parent, octant->childIndex);
                    octant = octant->parent;
                }
                return;
            }
        }
    }

    /// Create a new child octant.
    Octant* CreateChildOctant(Octant* octant, unsigned char index);
    /// Delete one child octant.
    void DeleteChildOctant(Octant* octant, unsigned char index);
    /// Delete a child octant hierarchy. If not deleting the octree for good, moves any nodes back to the root octant.
    void DeleteChildOctants(Octant* octant, bool deletingOctree);
    /// Return all drawables from an octant recursively.
    void CollectDrawables(std::vector<Drawable*>& result, Octant* octant) const;
    /// Return all drawables matching flags from an octant recursively.
    void CollectDrawables(std::vector<Drawable*>& result, Octant* octant, unsigned short drawableFlags, unsigned layerMask) const;
    /// Return all drawables matching flags along a ray.
    void CollectDrawables(std::vector<RaycastResult>& result, Octant* octant, const Ray& ray, unsigned short drawableFlags, float maxDistance, unsigned layerMask) const;
    /// Return all visible drawables matching flags that could be potential raycast hits.
    void CollectDrawables(std::vector<std::pair<Drawable*, float> >& result, Octant* octant, const Ray& ray, unsigned short drawableFlags, float maxDistance, unsigned layerMask) const;
    /// Work function to check reinsertion of nodes.
    void CheckReinsertWork(Task* task, unsigned threadIndex);

    /// Collect nodes matching flags using a volume such as frustum or sphere.
    template <class T> void CollectDrawables(std::vector<Drawable*>& result, Octant* octant, const T& volume, unsigned short drawableFlags, unsigned layerMask) const
    {
        Intersection res = volume.IsInside(octant->CullingBox());
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
            
            if (octant->numChildren)
            {
                for (size_t i = 0; i < NUM_OCTANTS; ++i)
                {
                    if (octant->children[i])
                        CollectDrawables(result, octant->children[i], volume, drawableFlags, layerMask);
                }
            }
        }
    }

    /// Collect nodes using a frustum and masked testing.
    void CollectDrawablesMasked(std::vector<Drawable*>& result, Octant* octant, const Frustum& frustum, unsigned short drawableFlags, unsigned layerMask, unsigned char planeMask = 0x3f) const
    {
        if (planeMask)
        {
            planeMask = frustum.IsInsideMasked(octant->CullingBox(), planeMask);
            // Terminate if octant completely outside frustum
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

        if (octant->numChildren)
        {
            for (size_t i = 0; i < NUM_OCTANTS; ++i)
            {
                if (octant->children[i])
                    CollectDrawablesMasked(result, octant->children[i], frustum, drawableFlags, layerMask, planeMask);
            }
        }
    }

    /// Threaded update flag. During threaded update moved drawables should go directly to thread-specific reinsert queues.
    volatile bool threadedUpdate;
    /// Current framenumber.
    unsigned short frameNumber;
    /// Queue of nodes to be reinserted.
    std::vector<Drawable*> updateQueue;
    /// Octants which need to have their drawables sorted.
    std::vector<Octant*> sortDirtyOctants;
    /// Extents of the octree root level box.
    BoundingBox worldBoundingBox;
    /// Root octant.
    Octant root;
    /// Allocator for child octants.
    Allocator<Octant> allocator;
    /// Cached %WorkQueue subsystem.
    WorkQueue* workQueue;
    /// Tasks for threaded reinsert execution.
    std::vector<AutoPtr<ReinsertDrawablesTask> > reinsertTasks;
    /// Intermediate reinsert queues for threaded execution.
    AutoArrayPtr<std::vector<Drawable*> > reinsertQueues;
    /// RaycastSingle initial coarse result.
    mutable std::vector<std::pair<Drawable*, float> > initialRayResult;
    /// RaycastSingle final result.
    mutable std::vector<RaycastResult> finalRayResult;
    /// Remaining drawable reinsertion tasks.
    std::atomic<int> numPendingReinsertionTasks;
};
