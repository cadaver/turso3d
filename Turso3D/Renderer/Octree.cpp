// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/Graphics.h"
#include "../IO/Log.h"
#include "../Math/Random.h"
#include "../Math/Ray.h"
#include "DebugRenderer.h"
#include "Octree.h"

#include <cassert>
#include <algorithm>
#include <tracy/Tracy.hpp>

static const float DEFAULT_OCTREE_SIZE = 1000.0f;
static const int DEFAULT_OCTREE_LEVELS = 8;
static const int MAX_OCTREE_LEVELS = 255;
static const size_t MIN_THREADED_UPDATE = 16;

static std::vector<unsigned> freeQueries;

static inline bool CompareRaycastResults(const RaycastResult& lhs, const RaycastResult& rhs)
{
    return lhs.distance < rhs.distance;
}

static inline bool CompareDrawableDistances(const std::pair<Drawable*, float>& lhs, const std::pair<Drawable*, float>& rhs)
{
    return lhs.second < rhs.second;
}

static inline bool CompareDrawables(Drawable* lhs, Drawable* rhs)
{
    unsigned short lhsFlags = lhs->Flags() & (DF_LIGHT | DF_GEOMETRY);
    unsigned short rhsFlags = rhs->Flags() & (DF_LIGHT | DF_GEOMETRY);
    if (lhsFlags != rhsFlags)
        return lhsFlags < rhsFlags;
    else
        return lhs < rhs;
}

/// %Task for octree drawables reinsertion.
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

Octant::Octant() :
    parent(nullptr),
    visibility(VIS_VISIBLE_UNKNOWN),
    occlusionQueryId(0),
    occlusionQueryTimer(Random() * OCCLUSION_QUERY_INTERVAL),
    numChildren(0)
{
    for (size_t i = 0; i < NUM_OCTANTS; ++i)
        children[i] = nullptr;
}

Octant::~Octant()
{
    if (occlusionQueryId)
    {
        Graphics* graphics = Object::Subsystem<Graphics>();
        if (graphics)
            graphics->FreeOcclusionQuery(occlusionQueryId);
    }
}

void Octant::Initialize(Octant* parent_, const BoundingBox& boundingBox, unsigned char level_, unsigned char childIndex_)
{
    BoundingBox worldBoundingBox = boundingBox;
    center = worldBoundingBox.Center();
    halfSize = worldBoundingBox.HalfSize();
    fittingBox = BoundingBox(worldBoundingBox.min - halfSize, worldBoundingBox.max + halfSize);

    parent = parent_;
    level = level_;
    childIndex = childIndex_;
    flags = OF_CULLING_BOX_DIRTY;
}

void Octant::OnRenderDebug(DebugRenderer* debug)
{
    debug->AddBoundingBox(CullingBox(), Color::GRAY, true);
}

void Octant::OnOcclusionQuery(unsigned queryId)
{
    // Should not have an existing query in flight
    assert(!occlusionQueryId);

    // Mark pending
    occlusionQueryId = queryId;
}

void Octant::OnOcclusionQueryResult(bool visible)
{
    // Mark not pending
    occlusionQueryId = 0;

    // Do not change visibility if currently outside the frustum
    if (visibility == VIS_OUTSIDE_FRUSTUM)
        return;

    OctantVisibility lastVisibility = (OctantVisibility)visibility;
    OctantVisibility newVisibility = visible ? VIS_VISIBLE : VIS_OCCLUDED;

    visibility = newVisibility;

    if (lastVisibility <= VIS_OCCLUDED_UNKNOWN && newVisibility == VIS_VISIBLE)
    {
        // If came into view after being occluded, mark children as still occluded but that should be tested in hierarchy
        if (numChildren)
            PushVisibilityToChildren(this, VIS_OCCLUDED_UNKNOWN);
    }
    else if (newVisibility == VIS_OCCLUDED && lastVisibility != VIS_OCCLUDED && parent && parent->visibility == VIS_VISIBLE)
    {
        // If became occluded, mark parent unknown so it will be tested next
        parent->visibility = VIS_VISIBLE_UNKNOWN;
    }

    // Whenever is visible, push visibility to parents if they are not visible yet
    if (newVisibility == VIS_VISIBLE)
    {
        Octant* octant = parent;

        while (octant && octant->visibility != newVisibility)
        {
            octant->visibility = newVisibility;
            octant = octant->parent;
        }
    }
}

const BoundingBox& Octant::CullingBox() const
{
    if (TestFlag(OF_CULLING_BOX_DIRTY))
    {
        if (!numChildren && drawables.empty())
            cullingBox.Define(center);
        else
        {
            // Use a temporary bounding box for calculations in case many threads call this simultaneously
            BoundingBox tempBox;

            for (auto it = drawables.begin(); it != drawables.end(); ++it)
                tempBox.Merge((*it)->WorldBoundingBox());

            if (numChildren)
            {
                for (size_t i = 0; i < NUM_OCTANTS; ++i)
                {
                    if (children[i])
                        tempBox.Merge(children[i]->CullingBox());
                }
            }

            cullingBox = tempBox;
        }

        SetFlag(OF_CULLING_BOX_DIRTY, false);
    }

    return cullingBox;
}

Octree::Octree() :
    threadedUpdate(false),
    frameNumber(0),
    workQueue(Subsystem<WorkQueue>())
{
    assert(workQueue);

    root.Initialize(nullptr, BoundingBox(-DEFAULT_OCTREE_SIZE, DEFAULT_OCTREE_SIZE), DEFAULT_OCTREE_LEVELS, 0);

    // Have at least 1 task for reinsert processing
    reinsertTasks.push_back(new ReinsertDrawablesTask(this, &Octree::CheckReinsertWork));
    reinsertQueues = new std::vector<Drawable*>[workQueue->NumThreads()];
}

Octree::~Octree()
{
    // Clear octree association from nodes that were never inserted
    // Note: the threaded queues cannot have nodes that were never inserted, only nodes that should be moved
    for (auto it = updateQueue.begin(); it != updateQueue.end(); ++it)
    {
        Drawable* drawable = *it;
        if (drawable)
        {
            drawable->octant = nullptr;
            drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
        }
    }

    DeleteChildOctants(&root, true);
}

void Octree::RegisterObject()
{
    // Register octree allocator with small initial capacity with the assumption that we don't create many of them (unlike other scene nodes)
    RegisterFactory<Octree>(1);
    CopyBaseAttributes<Octree, Node>();
    RegisterDerivedType<Octree, Node>();
    RegisterRefAttribute("boundingBox", &Octree::BoundingBoxAttr, &Octree::SetBoundingBoxAttr);
    RegisterAttribute("numLevels", &Octree::NumLevelsAttr, &Octree::SetNumLevelsAttr);
}

void Octree::Update(unsigned short frameNumber_)
{
    ZoneScoped;

    frameNumber = frameNumber_;

    // Avoid overhead of threaded update if only a small number of objects to update / reinsert
    if (updateQueue.size())
    {
        SetThreadedUpdate(true);

        // Split into smaller tasks to encourage work stealing in case some thread is slower
        size_t nodesPerTask = Max(MIN_THREADED_UPDATE, updateQueue.size() / workQueue->NumThreads() / 4);
        size_t taskIdx = 0;

        for (size_t start = 0; start < updateQueue.size(); start += nodesPerTask)
        {
            size_t end = start + nodesPerTask;
            if (end > updateQueue.size())
                end = updateQueue.size();

            if (reinsertTasks.size() <= taskIdx)
                reinsertTasks.push_back(new ReinsertDrawablesTask(this, &Octree::CheckReinsertWork));
            reinsertTasks[taskIdx]->start = &updateQueue[0] + start;
            reinsertTasks[taskIdx]->end = &updateQueue[0] + end;
            ++taskIdx;
        }

        numPendingReinsertionTasks.store((int)taskIdx);
        workQueue->QueueTasks(taskIdx, reinterpret_cast<Task**>(&reinsertTasks[0]));
    }
    else
        numPendingReinsertionTasks.store(0);
}

void Octree::FinishUpdate()
{
    ZoneScoped;

    // Complete tasks until reinsertions done. There may other tasks going on at the same time
    while (numPendingReinsertionTasks.load() > 0)
        workQueue->TryComplete();

    SetThreadedUpdate(false);

    // Now reinsert drawables that actually need reinsertion into a different octant
    for (size_t i = 0; i < workQueue->NumThreads(); ++i)
        ReinsertDrawables(reinsertQueues[i]);

    updateQueue.clear();

    // Sort octants' drawables by address and put lights first
    for (auto it = sortDirtyOctants.begin(); it != sortDirtyOctants.end(); ++it)
    {
        Octant* octant = *it;
        std::sort(octant->drawables.begin(), octant->drawables.end(), CompareDrawables);
        octant->SetFlag(OF_DRAWABLES_SORT_DIRTY, false);
    }

    sortDirtyOctants.clear();
}

void Octree::Resize(const BoundingBox& boundingBox, int numLevels)
{
    ZoneScoped;

    // Collect nodes to the root and delete all child octants
    updateQueue.clear();
    std::vector<Drawable*> occluders;
    
    CollectDrawables(updateQueue, &root);
    DeleteChildOctants(&root, false);

    allocator.Reset();
    root.Initialize(nullptr, boundingBox, (unsigned char)Clamp(numLevels, 1, MAX_OCTREE_LEVELS), 0);
}

void Octree::OnRenderDebug(DebugRenderer* debug)
{
    root.OnRenderDebug(debug);
}

void Octree::Raycast(std::vector<RaycastResult>& result, const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask) const
{
    ZoneScoped;

    result.clear();
    CollectDrawables(result, const_cast<Octant*>(&root), ray, nodeFlags, maxDistance, layerMask);
    std::sort(result.begin(), result.end(), CompareRaycastResults);
}

RaycastResult Octree::RaycastSingle(const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask) const
{
    ZoneScoped;

    // Get the potential hits first
    initialRayResult.clear();
    CollectDrawables(initialRayResult, const_cast<Octant*>(&root), ray, nodeFlags, maxDistance, layerMask);
    std::sort(initialRayResult.begin(), initialRayResult.end(), CompareDrawableDistances);

    // Then perform actual per-node ray tests and early-out when possible
    finalRayResult.clear();
    float closestHit = M_INFINITY;
    for (auto it = initialRayResult.begin(); it != initialRayResult.end(); ++it)
    {
        if (it->second < Min(closestHit, maxDistance))
        {
            size_t oldSize = finalRayResult.size();
            it->first->OnRaycast(finalRayResult, ray, maxDistance);
            if (finalRayResult.size() > oldSize)
                closestHit = Min(closestHit, finalRayResult.back().distance);
        }
        else
            break;
    }

    if (finalRayResult.size())
    {
        std::sort(finalRayResult.begin(), finalRayResult.end(), CompareRaycastResults);
        return finalRayResult.front();
    }
    else
    {
        RaycastResult emptyRes;
        emptyRes.position = emptyRes.normal = Vector3::ZERO;
        emptyRes.distance = M_INFINITY;
        emptyRes.drawable = nullptr;
        emptyRes.subObject = 0;
        return emptyRes;
    }
}

void Octree::FindDrawablesMasked(std::vector<Drawable*>& result, const Frustum& frustum, unsigned short drawableFlags, unsigned layerMask) const
{
    ZoneScoped;

    CollectDrawablesMasked(result, const_cast<Octant*>(&root), frustum, drawableFlags, layerMask);
}

void Octree::QueueUpdate(Drawable* drawable)
{
    assert(drawable);

    if (drawable->octant)
        drawable->octant->MarkCullingBoxDirty();

    if (!threadedUpdate)
    {
        updateQueue.push_back(drawable);
        drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, true);
    }
    else
    {
        drawable->lastUpdateFrameNumber = frameNumber;

        // Do nothing if still fits the current octant
        const BoundingBox& box = drawable->WorldBoundingBox();
        Octant* oldOctant = drawable->GetOctant();
        if (!oldOctant || oldOctant->fittingBox.IsInside(box) != INSIDE)
        {
            reinsertQueues[WorkQueue::ThreadIndex()].push_back(drawable);
            drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, true);
        }
    }
}

void Octree::RemoveDrawable(Drawable* drawable)
{
    if (!drawable)
        return;

    RemoveDrawable(drawable, drawable->GetOctant());
    if (drawable->TestFlag(DF_OCTREE_REINSERT_QUEUED))
    {
        RemoveDrawableFromQueue(drawable, updateQueue);

        // Remove also from threaded queues if was left over before next update
        for (size_t i = 0; i < workQueue->NumThreads(); ++i)
            RemoveDrawableFromQueue(drawable, reinsertQueues[i]);

        drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
    }

    drawable->octant = nullptr;
}

void Octree::SetBoundingBoxAttr(const BoundingBox& value)
{
    worldBoundingBox = value;
}

const BoundingBox& Octree::BoundingBoxAttr() const
{
    return worldBoundingBox;
}

void Octree::SetNumLevelsAttr(int numLevels)
{
    /// Setting the number of level (last attribute) triggers octree resize when deserializing
    Resize(worldBoundingBox, numLevels);
}

int Octree::NumLevelsAttr() const
{
    return root.level;
}

void Octree::ReinsertDrawables(std::vector<Drawable*>& drawables)
{
    for (auto it = drawables.begin(); it != drawables.end(); ++it)
    {
        Drawable* drawable = *it;

        const BoundingBox& box = drawable->WorldBoundingBox();
        Octant* oldOctant = drawable->GetOctant();
        Octant* newOctant = &root;
        Vector3 boxSize = box.Size();

        for (;;)
        {
            // If drawable does not fit fully inside root octant, must remain in it
            bool insertHere = (newOctant == &root) ?
                (newOctant->fittingBox.IsInside(box) != INSIDE || newOctant->FitBoundingBox(box, boxSize)) :
                newOctant->FitBoundingBox(box, boxSize);

            if (insertHere)
            {
                if (newOctant != oldOctant)
                {
                    // Add first, then remove, because drawable count going to zero deletes the octree branch in question
                    AddDrawable(drawable, newOctant);
                    if (oldOctant)
                        RemoveDrawable(drawable, oldOctant);
                }
                break;
            }
            else
                newOctant = CreateChildOctant(newOctant, newOctant->ChildIndex(box.Center()));
        }

        drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
    }

    drawables.clear();
}

void Octree::RemoveDrawableFromQueue(Drawable* drawable, std::vector<Drawable*>& drawables)
{
    for (auto it = drawables.begin(); it != drawables.end(); ++it)
    {
        if ((*it) == drawable)
        {
            *it = nullptr;
            break;
        }
    }
}

Octant* Octree::CreateChildOctant(Octant* octant, unsigned char index)
{
    if (octant->children[index])
        return octant->children[index];

    // Remove the culling extra from the bounding box before splitting
    Vector3 newMin = octant->fittingBox.min + octant->halfSize;
    Vector3 newMax = octant->fittingBox.max - octant->halfSize;
    const Vector3& oldCenter = octant->center;

    if (index & 1)
        newMin.x = oldCenter.x;
    else
        newMax.x = oldCenter.x;

    if (index & 2)
        newMin.y = oldCenter.y;
    else
        newMax.y = oldCenter.y;

    if (index & 4)
        newMin.z = oldCenter.z;
    else
        newMax.z = oldCenter.z;

    Octant* child = allocator.Allocate();
    child->Initialize(octant, BoundingBox(newMin, newMax), octant->level - 1, index);
    octant->children[index] = child;
    ++octant->numChildren;

    return child;
}

void Octree::DeleteChildOctant(Octant* octant, unsigned char index)
{
    allocator.Free(octant->children[index]);
    octant->children[index] = nullptr;
    --octant->numChildren;
}

void Octree::DeleteChildOctants(Octant* octant, bool deletingOctree)
{
    for (auto it = octant->drawables.begin(); it != octant->drawables.end(); ++it)
    {
        Drawable* drawable = *it;
        drawable->octant = nullptr;
        drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
        if (deletingOctree)
            drawable->Owner()->octree = nullptr;
    }
    octant->drawables.clear();

    if (octant->numChildren)
    {
        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
            {
                DeleteChildOctants(octant->children[i], deletingOctree);
                allocator.Free(octant->children[i]);
                octant->children[i] = nullptr;
            }
        }
        octant->numChildren = 0;
    }
}

void Octree::CollectDrawables(std::vector<Drawable*>& result, Octant* octant) const
{
    result.insert(result.end(), octant->drawables.begin(), octant->drawables.end());

    if (octant->numChildren)
    {
        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectDrawables(result, octant->children[i]);
        }
    }
}

void Octree::CollectDrawables(std::vector<Drawable*>& result, Octant* octant, unsigned short drawableFlags, unsigned layerMask) const
{
    std::vector<Drawable*>& drawables = octant->drawables;

    for (auto it = drawables.begin(); it != drawables.end(); ++it)
    {
        Drawable* drawable = *it;
        if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->LayerMask() & layerMask))
            result.push_back(drawable);
    }

    if (octant->numChildren)
    {
        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectDrawables(result, octant->children[i], drawableFlags, layerMask);
        }
    }
}

void Octree::CollectDrawables(std::vector<RaycastResult>& result, Octant* octant, const Ray& ray, unsigned short drawableFlags, float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->CullingBox());
    if (octantDist >= maxDistance)
        return;

    std::vector<Drawable*>& drawables = octant->drawables;

    for (auto it = drawables.begin(); it != drawables.end(); ++it)
    {
        Drawable* drawable = *it;
        if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->LayerMask() & layerMask))
            drawable->OnRaycast(result, ray, maxDistance);
    }

    if (octant->numChildren)
    {
        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectDrawables(result, octant->children[i], ray, drawableFlags, maxDistance, layerMask);
        }
    }
}

void Octree::CollectDrawables(std::vector<std::pair<Drawable*, float> >& result, Octant* octant, const Ray& ray, unsigned short drawableFlags, float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->CullingBox());
    if (octantDist >= maxDistance)
        return;

    std::vector<Drawable*>& drawables = octant->drawables;

    for (auto it = drawables.begin(); it != drawables.end(); ++it)
    {
        Drawable* drawable = *it;
        if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->LayerMask() & layerMask))
        {
            float distance = ray.HitDistance(drawable->WorldBoundingBox());
            if (distance < maxDistance)
                result.push_back(std::make_pair(drawable, distance));
        }
    }

    if (octant->numChildren)
    {
        for (size_t i = 0; i < NUM_OCTANTS; ++i)
        {
            if (octant->children[i])
                CollectDrawables(result, octant->children[i], ray, drawableFlags, maxDistance, layerMask);
        }
    }
}

void Octree::CheckReinsertWork(Task* task_, unsigned threadIndex_)
{
    ZoneScoped;

    ReinsertDrawablesTask* task = static_cast<ReinsertDrawablesTask*>(task_);
    Drawable** start = task->start;
    Drawable** end = task->end;
    std::vector<Drawable*>& reinsertQueue = reinsertQueues[threadIndex_];

    for (; start != end; ++start)
    {
        // If drawable was removed before reinsertion could happen, a null pointer will be in its place
        Drawable* drawable = *start;
        if (!drawable)
            continue;

        if (drawable->TestFlag(DF_OCTREE_UPDATE_CALL))
            drawable->OnOctreeUpdate(frameNumber);

        drawable->lastUpdateFrameNumber = frameNumber;

        // Do nothing if still fits the current octant
        const BoundingBox& box = drawable->WorldBoundingBox();
        Octant* oldOctant = drawable->GetOctant();
        if (!oldOctant || oldOctant->fittingBox.IsInside(box) != INSIDE)
            reinsertQueue.push_back(drawable);
        else
            drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
    }

    numPendingReinsertionTasks.fetch_add(-1);
}
