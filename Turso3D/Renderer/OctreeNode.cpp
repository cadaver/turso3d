// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/Ray.h"
#include "../Scene/Scene.h"
#include "Octree.h"

namespace Turso3D
{

OctreeNode::OctreeNode() :
    octree(nullptr),
    octant(nullptr)
{
    SetFlag(NF_BOUNDING_BOX_DIRTY, true);
}

OctreeNode::~OctreeNode()
{
    RemoveFromOctree();
}

void OctreeNode::OnRaycast(Vector<RaycastResult>& dest, const Ray& ray, float maxDistance)
{
    float distance = ray.HitDistance(WorldBoundingBox());
    if (distance < maxDistance)
    {
        RaycastResult res;
        res.position = ray.origin + distance * ray.direction;
        res.normal = -ray.direction;
        res.distance = distance;
        res.node = this;
        res.extraData = 0;
        dest.Push(res);
    }
}

void OctreeNode::OnSceneSet(Scene* newScene, Scene*)
{
    /// Remove from current octree if any
    RemoveFromOctree();

    if (newScene)
    {
        // Octree must be attached to the scene root as a child
        octree = newScene->FindChild<Octree>();
        // Transform may not be final yet. Schedule update but do not insert into octree yet
        if (octree)
            octree->QueueUpdate(this);
    }
}

void OctreeNode::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();
    SetFlag(NF_BOUNDING_BOX_DIRTY, true);

    if (!TestFlag(NF_OCTREE_UPDATE_QUEUED) && octree)
        octree->QueueUpdate(this);
}

void OctreeNode::OnWorldBoundingBoxUpdate() const
{
    // The OctreeNode base class does not have a defined size, so represent as a point
    worldBoundingBox.Define(WorldPosition());
    SetFlag(NF_BOUNDING_BOX_DIRTY, false);
}

void OctreeNode::RemoveFromOctree()
{
    if (octree)
    {
        octree->RemoveNode(this);
        octree = nullptr;
    }
}

}