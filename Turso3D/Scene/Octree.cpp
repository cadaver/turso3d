// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Math/Ray.h"
#include "Octree.h"

#include <cassert>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static const float DEFAULT_OCTREE_SIZE = 1000.0f;
static const int DEFAULT_OCTREE_LEVELS = 8;
static const int MAX_OCTREE_LEVELS = 256;

bool CompareRaycastResults(const RaycastResult& lhs, const RaycastResult& rhs)
{
    return lhs.distance < rhs.distance;
}

bool CompareNodeDistances(const Pair<OctreeNode*, float>& lhs, const Pair<OctreeNode*, float>& rhs)
{
    return lhs.second < rhs.second;
}

Octant::Octant() :
    parent(0),
    numNodes(0)
{
    for (size_t i = 0; i < NUM_OCTANTS; ++i)
        children[i] = 0;
}

void Octant::Initialize(Octant* parent_, const BoundingBox& boundingBox, int level_)
{
    worldBoundingBox = boundingBox;
    center = worldBoundingBox.Center();
    halfSize = worldBoundingBox.HalfSize();
    cullingBox = BoundingBox(worldBoundingBox.min - halfSize, worldBoundingBox.max + halfSize);
    level = level_;
    parent = parent_;
}

bool Octant::FitBoundingBox(const BoundingBox& box, const Vector3& boxSize) const
{
    // If max split level, size always OK, otherwise check that box is at least half size of octant
    if (level <= 1 || boxSize.x >= halfSize.x || boxSize.y >= halfSize.y || boxSize.z >= halfSize.z)
        return true;
    // Also check if the box can not fit inside a child octant's culling box, in that case size OK (must insert here)
    else
    {
        if (box.min.x <= worldBoundingBox.min.x - 0.5f * halfSize.x || box.min.y <= worldBoundingBox.min.y - 0.5f * halfSize.y ||
            box.min.z <= worldBoundingBox.min.z - 0.5f * halfSize.z || box.max.x >= worldBoundingBox.max.x + 0.5f * halfSize.x ||
            box.max.y >= worldBoundingBox.max.y + 0.5f * halfSize.y || box.max.z >= worldBoundingBox.max.z + 0.5f * halfSize.z)
            return true;
    }

    // Bounding box too small, should create a child octant
    return false;
}

Octree::Octree()
{
    root.Initialize(0, BoundingBox(-DEFAULT_OCTREE_SIZE, DEFAULT_OCTREE_SIZE), DEFAULT_OCTREE_LEVELS);
}

Octree::~Octree()
{
    DeleteChildOctants(&root, true);
}

void Octree::RegisterObject()
{
    RegisterFactory<Octree>();
    CopyBaseAttributes<Octree, Node>();
    RegisterRefAttribute("boundingBox", &Octree::BoundingBoxAttr, &Octree::SetBoundingBoxAttr);
    RegisterAttribute("numLevels", &Octree::NumLevelsAttr, &Octree::SetNumLevelsAttr);
}

void Octree::Update()
{
    PROFILE(UpdateOctree);

    for (Vector<OctreeNode*>::Iterator it = updateQueue.Begin(); it != updateQueue.End(); ++it)
    {
        OctreeNode* node = *it;
        // If node was removed before update could happen, a null pointer will be in its place
        if (node)
            InsertNode(node);
    }

    updateQueue.Clear();
}

void Octree::Resize(const BoundingBox& boundingBox, int numLevels)
{
    PROFILE(ResizeOctree);

    // Collect nodes to the root and delete all child octants
    updateQueue.Clear();
    CollectNodes(updateQueue, &root);
    DeleteChildOctants(&root, false);
    allocator.Reset();
    root.Initialize(0, boundingBox, Clamp(numLevels, 1, MAX_OCTREE_LEVELS));

    // Reinsert all nodes (recreates new child octants as necessary)
    Update();
}

void Octree::RemoveNode(OctreeNode* node)
{
    assert(node);
    RemoveNode(node, node->octant);
    if (node->TestFlag(NF_OCTREE_UPDATE_QUEUED))
        CancelUpdate(node);
    node->octant = 0;
}

void Octree::QueueUpdate(OctreeNode* node)
{
    assert(node);
    updateQueue.Push(node);
    node->SetFlag(NF_OCTREE_UPDATE_QUEUED, true);
}

void Octree::CancelUpdate(OctreeNode* node)
{
    assert(node);
    Vector<OctreeNode*>::Iterator it = updateQueue.Find(node);
    if (it != updateQueue.End())
        *it = 0;
    node->SetFlag(NF_OCTREE_UPDATE_QUEUED, false);
}

void Octree::Raycast(Vector<RaycastResult>& result, const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask)
{
    PROFILE(OctreeRaycast);

    result.Clear();
    CollectNodes(result, &root, ray, nodeFlags, maxDistance, layerMask);
    Sort(result.Begin(), result.End(), CompareRaycastResults);
}

RaycastResult Octree::RaycastSingle(const Ray& ray, unsigned short nodeFlags, float maxDistance, unsigned layerMask)
{
    PROFILE(OctreeRaycastSingle);

    // Get first the potential hits
    initialRes.Clear();
    CollectNodes(initialRes, &root, ray, nodeFlags, maxDistance, layerMask);
    Sort(initialRes.Begin(), initialRes.End(), CompareNodeDistances);

    // Then perform actual per-node ray tests and early-out when possible
    finalRes.Clear();
    float closestHit = M_INFINITY;
    for (Vector<Pair<OctreeNode*, float> >::ConstIterator it = initialRes.Begin(); it != initialRes.End(); ++it)
    {
        if (it->second < Min(closestHit, maxDistance))
        {
            size_t oldSize = finalRes.Size();
            it->first->OnRaycast(finalRes, ray, maxDistance);
            if (finalRes.Size() > oldSize)
                closestHit = Min(closestHit, finalRes.Back().distance);
        }
        else
            break;
    }

    if (finalRes.Size())
    {
        Sort(finalRes.Begin(), finalRes.End(), CompareRaycastResults);
        return finalRes.Front();
    }
    else
    {
        RaycastResult emptyRes;
        emptyRes.position = emptyRes.normal = Vector3::ZERO;
        emptyRes.distance = M_INFINITY;
        emptyRes.node = 0;
        emptyRes.extraData = 0;
        return emptyRes;
    }
}

void Octree::SetBoundingBoxAttr(const BoundingBox& boundingBox)
{
    root.worldBoundingBox = boundingBox;
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

void Octree::InsertNode(OctreeNode* node)
{
    node->SetFlag(NF_OCTREE_UPDATE_QUEUED, false);

    // Do nothing if still fits the current octant
    const BoundingBox& box = node->WorldBoundingBox();
    Vector3 boxSize = box.Size();
    Octant* oldOctant = node->octant;
    if (oldOctant && oldOctant->cullingBox.IsInside(box) == INSIDE && oldOctant->FitBoundingBox(box, boxSize))
        return;

    // Begin reinsert process. Start from root and check what level child needs to be used
    Octant* newOctant = &root;
    Vector3 boxCenter = box.Center();

    for (;;)
    {
        bool insertHere;
        // If node does not fit fully inside root octant, must remain in it
        if (newOctant == &root)
            insertHere = newOctant->cullingBox.IsInside(box) != INSIDE || newOctant->FitBoundingBox(box, boxSize);
        else
            insertHere = newOctant->FitBoundingBox(box, boxSize);

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
            newOctant = CreateChildOctant(newOctant, newOctant->ChildIndex(boxCenter));
    }
}

void Octree::AddNode(OctreeNode* node, Octant* octant)
{
    octant->nodes.Push(node);
    node->octant = octant;

    // Increment the node count in the whole parent branch
    while (octant)
    {
        ++octant->numNodes;
        octant = octant->parent;
    }
}

void Octree::RemoveNode(OctreeNode* node, Octant* octant)
{
    // Do not set the node's octant pointer to zero, as the node may already be added into another octant
    octant->nodes.Remove(node);
    
    // Decrement the node count in the whole parent branch and erase empty octants as necessary
    while (octant)
    {
        --octant->numNodes;
        Octant* next = octant->parent;
        if (!octant->numNodes && next)
            DeleteChildOctant(next, next->ChildIndex(octant->center));
        octant = next;
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
    octant->children[index] = 0;
}

void Octree::DeleteChildOctants(Octant* octant, bool deletingOctree)
{
    for (Vector<OctreeNode*>::Iterator it = octant->nodes.Begin(); it != octant->nodes.End(); ++it)
    {
        OctreeNode* node = *it;
        node->octant = 0;
        node->SetFlag(NF_OCTREE_UPDATE_QUEUED, false);
        if (deletingOctree)
            node->octree = 0;
    }
    octant->nodes.Clear();
    octant->numNodes = 0;

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
        {
            DeleteChildOctants(octant->children[i], deletingOctree);
            octant->children[i] = 0;
        }
    }

    if (octant != &root)
        allocator.Free(octant);
}

void Octree::CollectNodes(Vector<OctreeNode*>& result, const Octant* octant) const
{
    result.Push(octant->nodes);

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i]);
    }
}

void Octree::CollectNodes(Vector<OctreeNode*>& result, const Octant* octant, unsigned short nodeFlags, unsigned layerMask) const
{
    const Vector<OctreeNode*>& octantNodes = octant->nodes;
    for (Vector<OctreeNode*>::ConstIterator it = octantNodes.Begin(); it != octantNodes.End(); ++it)
    {
        OctreeNode* node = *it;
        unsigned flags = node->Flags();
        if ((flags & NF_ENABLED) && (flags & nodeFlags) && (node->LayerMask() & layerMask))
            result.Push(node);
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], nodeFlags, layerMask);
    }
}

void Octree::CollectNodes(Vector<RaycastResult>& result, const Octant* octant, const Ray& ray, unsigned short nodeFlags, 
    float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->cullingBox);
    if (octantDist >= maxDistance)
        return;

    const Vector<OctreeNode*>& octantNodes = octant->nodes;
    for (Vector<OctreeNode*>::ConstIterator it = octantNodes.Begin(); it != octantNodes.End(); ++it)
    {
        OctreeNode* node = *it;
        unsigned flags = node->Flags();
        if ((flags & NF_ENABLED) && (flags & nodeFlags) && (node->LayerMask() & layerMask))
            node->OnRaycast(result, ray, maxDistance);
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], ray, nodeFlags, maxDistance, layerMask);
    }
}

void Octree::CollectNodes(Vector<Pair<OctreeNode*, float> >& result, const Octant* octant, const Ray& ray, unsigned short nodeFlags,
    float maxDistance, unsigned layerMask) const
{
    float octantDist = ray.HitDistance(octant->cullingBox);
    if (octantDist >= maxDistance)
        return;

    const Vector<OctreeNode*>& octantNodes = octant->nodes;
    for (Vector<OctreeNode*>::ConstIterator it = octantNodes.Begin(); it != octantNodes.End(); ++it)
    {
        OctreeNode* node = *it;
        unsigned flags = node->Flags();
        if ((flags & NF_ENABLED) && (flags & nodeFlags) && (node->LayerMask() & layerMask))
        {
            float distance = ray.HitDistance(node->WorldBoundingBox());
            if (distance < maxDistance)
                result.Push(MakePair(node, distance));
        }
    }

    for (size_t i = 0; i < NUM_OCTANTS; ++i)
    {
        if (octant->children[i])
            CollectNodes(result, octant->children[i], ray, nodeFlags, maxDistance, layerMask);
    }
}

}
