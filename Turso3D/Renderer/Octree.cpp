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

bool CompareRaycastResults(const RaycastResult& lhs, const RaycastResult& rhs)
{
    return lhs.distance < rhs.distance;
}

bool CompareNodeDistances(const std::pair<OctreeNode*, float>& lhs, const std::pair<OctreeNode*, float>& rhs)
{
    return lhs.second < rhs.second;
}

bool CompareOctantNodes(OctreeNode* lhs, OctreeNode* rhs)
{
    unsigned short lhsFlags = lhs->Flags() & (NF_LIGHT | NF_GEOMETRY);
    unsigned short rhsFlags = rhs->Flags() & (NF_LIGHT | NF_GEOMETRY);
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
    numNodes = 0;

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
    reinsertTasks.push_back(new MemberFunctionTask<Octree>(this, &Octree::CheckReinsertWork));
    reinsertQueues.resize(workQueue->NumThreads());
}

Octree::~Octree()
{
    // Clear octree association from nodes that were never inserted
    // Note: the threaded queues cannot have nodes that were never inserted, only nodes that should be moved
    for (auto it = updateQueue.begin(); it != updateQueue.end(); ++it)
    {
        OctreeNode* node = *it;
        if (node && node->octree == this && !node->octant)
        {
            node->octree = nullptr;
            node->SetFlag(NF_OCTREE_REINSERT_QUEUED, false);
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
    const size_t minThreadedUpdate = 16;
    // Split into smaller tasks to encourage work stealing in case some thread is slower
    const size_t nodesPerTask = Max(1, (unsigned)updateQueue.size() / workQueue->NumThreads() / 4);

    if (updateQueue.size() >= minThreadedUpdate)
    {
        SetThreadedUpdate(true);

        size_t taskIdx = 0;

        for (size_t start = 0; start < updateQueue.size(); start += nodesPerTask)
        {
            size_t end = start + nodesPerTask;
            if (end > updateQueue.size())
                end = updateQueue.size();

            if (reinsertTasks.size() <= taskIdx)
                reinsertTasks.push_back(new MemberFunctionTask<Octree>(this, &Octree::CheckReinsertWork));
            reinsertTasks[taskIdx]->start = &updateQueue[0] + start;
            reinsertTasks[taskIdx]->end = &updateQueue[0] + end;
            ++taskIdx;
        }

        workQueue->QueueTasks(taskIdx, reinterpret_cast<Task**>(&reinsertTasks[0]));
        workQueue->Complete();

        SetThreadedUpdate(false);

        for (auto it = reinsertQueues.begin(); it != reinsertQueues.end(); ++it)
            ReinsertNodes(*it);

    }
    else if (updateQueue.size())
    {
        reinsertTasks[0]->start = &updateQueue[0];
        reinsertTasks[0]->end = &updateQueue[0] + updateQueue.size();
        reinsertTasks[0]->Invoke(reinsertTasks[0], 0);
        ReinsertNodes(reinsertQueues[0]);
    }

    updateQueue.clear();

    // Sort octants' nodes by address and put lights first
    for (auto it = sortDirtyOctants.begin(); it != sortDirtyOctants.end(); ++it)
    {
        Octant* octant = *it;
        std::sort(octant->nodes.begin(), octant->nodes.end(), CompareOctantNodes);
        octant->sortDirty = false;
    }

    sortDirtyOctants.clear();
}

void Octree::Resize(const BoundingBox& boundingBox, int numLevels)
{
    ZoneScoped;

    // Collect nodes to the root and delete all child octants
    updateQueue.clear();
    CollectNodes(updateQueue, &root);
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
    result.clear();
    CollectNodes(result, const_cast<Octant*>(&root), ray, nodeFlags, maxDistance, layerMask);
    std::sort(result.begin(), result.end(), CompareRaycastResults);
}

RaycastResult Octree::RaycastSingle(const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask) const
{
    // Get first the potential hits
    initialRayResult.clear();
    CollectNodes(initialRayResult, const_cast<Octant*>(&root), ray, nodeFlags, maxDistance, layerMask);
    std::sort(initialRayResult.begin(), initialRayResult.end(), CompareNodeDistances);

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

void Octree::RemoveNode(OctreeNode* node)
{
    assert(node);

    RemoveNode(node, node->octant);
    if (node->TestFlag(NF_OCTREE_REINSERT_QUEUED))
    {
        RemoveNodeFromQueue(node, updateQueue);

        // Remove also from threaded queues if was left over before next update
        for (size_t i = 0; i < reinsertQueues.size(); ++i)
            RemoveNodeFromQueue(node, reinsertQueues[i]);

        node->SetFlag(NF_OCTREE_REINSERT_QUEUED, false);
    }

    node->octant = nullptr;
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

void Octree::ReinsertNodes(std::vector<OctreeNode*>& nodes)
{
    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    {
        OctreeNode* node = *it;

        const BoundingBox& box = node->WorldBoundingBox();
        Octant* oldOctant = node->octant;
        Octant* newOctant = &root;
        Vector3 boxSize = box.Size();

        for (;;)
        {
            // If node does not fit fully inside root octant, must remain in it
            bool insertHere = (newOctant == &root) ?
                (newOctant->cullingBox.IsInside(box) != INSIDE || newOctant->FitBoundingBox(box, boxSize)) :
                newOctant->FitBoundingBox(box, boxSize);

            if (insertHere)
            {
                if (newOctant != oldOctant)
                {
                    // Add first, then remove, because node count going to zero deletes the octree branch in question
                    AddNode(node, newOctant);
                    if (oldOctant)
                        RemoveNode(node, oldOctant);
                }
                break;
            }
            else
                newOctant = CreateChildOctant(newOctant, newOctant->ChildIndex(box.Center()));
        }

        node->SetFlag(NF_OCTREE_REINSERT_QUEUED, false);
    }

    nodes.clear();
}

void Octree::RemoveNodeFromQueue(OctreeNode* node, std::vector<OctreeNode*>& nodes)
{
    for (auto it = nodes.begin(); it != nodes.end(); ++it)
    {
        if ((*it) == node)
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
    for (auto it = octant->nodes.begin(); it != octant->nodes.end(); ++it)
    {
        OctreeNode* node = *it;
        node->octant = nullptr;
        node->SetFlag(NF_OCTREE_REINSERT_QUEUED, false);
        if (deletingOctree)
            node->octree = nullptr;
    }
    octant->nodes.clear();
    octant->numNodes = 0;

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

void Octree::CollectNodes(std::vector<OctreeNode*>& result, Octant* octant) const
{
    result.insert(result.end(), octant->nodes.begin(), octant->nodes.end());

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i]);
    }
}

void Octree::CollectNodes(std::vector<OctreeNode*>& result, Octant* octant, unsigned short nodeFlags, unsigned layerMask) const
{
    std::vector<OctreeNode*>& octantNodes = octant->nodes;

    for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
    {
        OctreeNode* node = *it;
        if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask))
            result.push_back(node);
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], nodeFlags, layerMask);
    }
}

void Octree::CollectNodes(std::vector<RaycastResult>& result, Octant* octant, const Ray& ray, unsigned short nodeFlags, 
    float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->cullingBox);
    if (octantDist >= maxDistance)
        return;

    std::vector<OctreeNode*>& octantNodes = octant->nodes;

    for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
    {
        OctreeNode* node = *it;
        if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask))
            node->OnRaycast(result, ray, maxDistance);
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], ray, nodeFlags, maxDistance, layerMask);
    }
}

void Octree::CollectNodes(std::vector<std::pair<OctreeNode*, float> >& result, Octant* octant, const Ray& ray, unsigned short nodeFlags,
    float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->cullingBox);
    if (octantDist >= maxDistance)
        return;

    std::vector<OctreeNode*>& octantNodes = octant->nodes;

    for (auto it = octantNodes.begin(); it != octantNodes.end(); ++it)
    {
        OctreeNode* node = *it;
        if ((node->Flags() & nodeFlags) == nodeFlags && (node->LayerMask() & layerMask))
        {
            float distance = ray.HitDistance(node->WorldBoundingBox());
            if (distance < maxDistance)
                result.push_back(std::make_pair(node, distance));
        }
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], ray, nodeFlags, maxDistance, layerMask);
    }
}

void Octree::CheckReinsertWork(Task* task, unsigned threadIndex_)
{
    ZoneScoped;

    OctreeNode** start = reinterpret_cast<OctreeNode**>(task->start);
    OctreeNode** end = reinterpret_cast<OctreeNode**>(task->end);
    std::vector<OctreeNode*>& reinsertQueue = reinsertQueues[threadIndex_];

    for (; start != end; ++start)
    {
        // If node was removed before update could happen, a null pointer will be in its place
        OctreeNode* node = *start;
        if (!node)
            continue;

        if (node->TestFlag(NF_OCTREE_UPDATE_CALL))
            node->OnOctreeUpdate(frameNumber);

        node->lastUpdateFrameNumber = frameNumber;

        // Do nothing if still fits the current octant
        const BoundingBox& box = node->WorldBoundingBox();
        Octant* oldOctant = node->octant;
        if (!oldOctant || oldOctant->cullingBox.IsInside(box) != INSIDE)
            reinsertQueue.push_back(node);
        else
            node->SetFlag(NF_OCTREE_REINSERT_QUEUED, false);
    }
}
