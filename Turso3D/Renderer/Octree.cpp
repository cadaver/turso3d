// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Math/Ray.h"
#include "Octree.h"

#include <cassert>
#include <algorithm>
#include <tracy/Tracy.hpp>

static const float DEFAULT_OCTREE_SIZE = 1000.0f;
static const int DEFAULT_OCTREE_LEVELS = 8;
static const int MAX_OCTREE_LEVELS = 255;

static const size_t MIN_THREADED_UPDATE = 16;

bool CompareRaycastResults(const RaycastResult& lhs, const RaycastResult& rhs)
{
    return lhs.distance < rhs.distance;
}

bool CompareDrawableDistances(const std::pair<Drawable*, float>& lhs, const std::pair<Drawable*, float>& rhs)
{
    return lhs.second < rhs.second;
}

bool CompareDrawables(Drawable* lhs, Drawable* rhs)
{
    unsigned short lhsFlags = lhs->Flags() & (DF_LIGHT | DF_GEOMETRY);
    unsigned short rhsFlags = rhs->Flags() & (DF_LIGHT | DF_GEOMETRY);
    if (lhsFlags != rhsFlags)
        return lhsFlags < rhsFlags;
    else
        return lhs < rhs;
}

void Octant::Initialize(Octant* parent_, const BoundingBox& boundingBox, unsigned char level_)
{
    worldBoundingBox = boundingBox;
    halfSize = worldBoundingBox.HalfSize();
    cullingBox = BoundingBox(worldBoundingBox.min - halfSize, worldBoundingBox.max + halfSize);
    center = worldBoundingBox.Center();
    level = level_;
    sortDirty = false;
    parent = parent_;
    numDrawables = 0;

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
        children[i] = nullptr;
}

bool Octant::FitBoundingBox(const BoundingBox& box, const Vector3& boxSize) const
{
    // If max split level, size always OK, otherwise check that box is at least half size of octant
    if (level <= 1 || boxSize.x >= halfSize.x || boxSize.y >= halfSize.y || boxSize.z >= halfSize.z)
        return true;
    // Also check if the box can not fit inside a child octant's culling box, in that case size OK (must insert here)
    else
    {
        Vector3 quarterSize = 0.5f * halfSize;
        if (box.min.x <= worldBoundingBox.min.x - quarterSize.x || box.max.x >= worldBoundingBox.max.x + quarterSize.x ||
            box.min.y <= worldBoundingBox.min.y - quarterSize.y || box.max.y >= worldBoundingBox.max.y + quarterSize.y ||
            box.max.z <= worldBoundingBox.min.z - quarterSize.z || box.max.z >= worldBoundingBox.max.z + quarterSize.z)
            return true;
    }

    // Bounding box too small, should create a child octant
    return false;
}

Octree::Octree() :
    threadedUpdate(false),
    frameNumber(0),
    workQueue(Subsystem<WorkQueue>())
{
    assert(workQueue);

    root.Initialize(nullptr, BoundingBox(-DEFAULT_OCTREE_SIZE, DEFAULT_OCTREE_SIZE), DEFAULT_OCTREE_LEVELS);

    // Have at least 1 task for reinsert processing
    reinsertTasks.push_back(new ReinsertDrawablesTask(this, &Octree::CheckReinsertWork));
    reinsertQueues.resize(workQueue->NumThreads());
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
            drawable->SetOctant(nullptr);
            drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
            drawable->Owner()->octree = nullptr;
        }
    }

    DeleteChildOctants(&root, true);
}

void Octree::RegisterObject()
{
    RegisterFactory<Octree>();
    CopyBaseAttributes<Octree, Node>();
    RegisterRefAttribute("boundingBox", &Octree::BoundingBoxAttr, &Octree::SetBoundingBoxAttr);
    RegisterAttribute("numLevels", &Octree::NumLevelsAttr, &Octree::SetNumLevelsAttr);
}

void Octree::Update(unsigned short frameNumber_)
{
    ZoneScoped;

    frameNumber = frameNumber_;

    // Avoid overhead of threaded update if only a small number of objects to update / reinsert
    if (updateQueue.size() >= MIN_THREADED_UPDATE)
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

        workQueue->QueueTasks(taskIdx, reinterpret_cast<Task**>(&reinsertTasks[0]));
        workQueue->Complete();

        SetThreadedUpdate(false);

        for (auto it = reinsertQueues.begin(); it != reinsertQueues.end(); ++it)
            ReinsertDrawables(*it);
    }
    else if (updateQueue.size())
    {
        // Execute a complete queue reinsert manually
        reinsertTasks[0]->start = &updateQueue[0];
        reinsertTasks[0]->end = &updateQueue[0] + updateQueue.size();
        reinsertTasks[0]->Complete(0);
        ReinsertDrawables(reinsertQueues[0]);
    }

    updateQueue.clear();

    // Sort octants' nodes by address and put lights first
    for (auto it = sortDirtyOctants.begin(); it != sortDirtyOctants.end(); ++it)
    {
        Octant* octant = *it;
        std::sort(octant->drawables.begin(), octant->drawables.end(), CompareDrawables);
        octant->sortDirty = false;
    }

    sortDirtyOctants.clear();
}

void Octree::Resize(const BoundingBox& boundingBox, int numLevels)
{
    ZoneScoped;

    // Collect nodes to the root and delete all child octants
    updateQueue.clear();
    CollectDrawables(updateQueue, &root);
    DeleteChildOctants(&root, false);
    allocator.Reset();
    root.Initialize(nullptr, boundingBox, (unsigned char)Clamp(numLevels, 1, MAX_OCTREE_LEVELS));

    // Nodes will be reinserted on next update
}

void Octree::SetThreadedUpdate(bool enable)
{
    threadedUpdate = enable;
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

    // Get first the potential hits
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
        emptyRes.node = nullptr;
        emptyRes.subObject = 0;
        return emptyRes;
    }
}

void Octree::FindDrawablesMasked(std::vector<Drawable*>& result, const Frustum& frustum, unsigned short drawableFlags, unsigned layerMask) const
{
    ZoneScoped;

    CollectDrawablesMasked(result, const_cast<Octant*>(&root), frustum, drawableFlags, layerMask);
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
        for (size_t i = 0; i < reinsertQueues.size(); ++i)
            RemoveDrawableFromQueue(drawable, reinsertQueues[i]);

        drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
    }

    drawable->SetOctant(nullptr);
}

void Octree::SetBoundingBoxAttr(const BoundingBox& value)
{
    root.worldBoundingBox = value;
}

const BoundingBox& Octree::BoundingBoxAttr() const
{
    return root.worldBoundingBox;
}

void Octree::SetNumLevelsAttr(int numLevels)
{
    /// Setting the number of level (last attribute) triggers octree resize when deserializing
    Resize(root.worldBoundingBox, numLevels);
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
                (newOctant->cullingBox.IsInside(box) != INSIDE || newOctant->FitBoundingBox(box, boxSize)) :
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

Octant* Octree::CreateChildOctant(Octant* octant, size_t index)
{
    if (octant->children[index])
        return octant->children[index];

    Vector3 newMin = octant->worldBoundingBox.min;
    Vector3 newMax = octant->worldBoundingBox.max;
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
    child->Initialize(octant, BoundingBox(newMin, newMax), octant->level - 1);
    octant->children[index] = child;

    return child;
}

void Octree::DeleteChildOctant(Octant* octant, size_t index)
{
    allocator.Free(octant->children[index]);
    octant->children[index] = nullptr;
}

void Octree::DeleteChildOctants(Octant* octant, bool deletingOctree)
{
    for (auto it = octant->drawables.begin(); it != octant->drawables.end(); ++it)
    {
        Drawable* drawable = *it;
        drawable->SetOctant(nullptr);
        drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
        if (deletingOctree)
            drawable->Owner()->octree = nullptr;
    }
    octant->drawables.clear();
    octant->numDrawables = 0;

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
        {
            DeleteChildOctants(octant->children[i], deletingOctree);
            octant->children[i] = nullptr;
        }
    }

    if (octant != &root)
        allocator.Free(octant);
}

void Octree::CollectDrawables(std::vector<Drawable*>& result, Octant* octant) const
{
    result.insert(result.end(), octant->drawables.begin(), octant->drawables.end());

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectDrawables(result, octant->children[i]);
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

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectDrawables(result, octant->children[i], drawableFlags, layerMask);
    }
}

void Octree::CollectDrawables(std::vector<RaycastResult>& result, Octant* octant, const Ray& ray, unsigned short drawableFlags, float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->cullingBox);
    if (octantDist >= maxDistance)
        return;

    std::vector<Drawable*>& drawables = octant->drawables;

    for (auto it = drawables.begin(); it != drawables.end(); ++it)
    {
        Drawable* drawable = *it;
        if ((drawable->Flags() & drawableFlags) == drawableFlags && (drawable->LayerMask() & layerMask))
            drawable->OnRaycast(result, ray, maxDistance);
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectDrawables(result, octant->children[i], ray, drawableFlags, maxDistance, layerMask);
    }
}

void Octree::CollectDrawables(std::vector<std::pair<Drawable*, float> >& result, Octant* octant, const Ray& ray, unsigned short drawableFlags, float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->cullingBox);
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

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectDrawables(result, octant->children[i], ray, drawableFlags, maxDistance, layerMask);
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

        drawable->SetLastUpdateFrameNumber(frameNumber);

        // Do nothing if still fits the current octant
        const BoundingBox& box = drawable->WorldBoundingBox();
        Octant* oldOctant = drawable->GetOctant();
        if (!oldOctant || oldOctant->cullingBox.IsInside(box) != INSIDE)
            reinsertQueue.push_back(drawable);
        else
            drawable->SetFlag(DF_OCTREE_REINSERT_QUEUED, false);
    }
}
