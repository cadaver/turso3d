// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/Ray.h"
#include "../Scene/Scene.h"
#include "Camera.h"
#include "Octree.h"
#include "OctreeNode.h"

OctreeNode::OctreeNode() :
    lastFrameNumber(0),
    lastUpdateFrameNumber(0),
    distance(0.0f),
    maxDistance(0.0f)
{
    octree = nullptr;
    octant = nullptr;
    SetFlag(NF_BOUNDING_BOX_DIRTY, true);
}

OctreeNode::~OctreeNode()
{
    RemoveFromOctree();
}

void OctreeNode::RegisterObject()
{
    CopyBaseAttributes<OctreeNode, SpatialNode>();
    RegisterDerivedType<OctreeNode, SpatialNode>();
    RegisterAttribute("castShadows", &OctreeNode::CastShadows, &OctreeNode::SetCastShadows, false);
    RegisterAttribute("maxDistance", &OctreeNode::MaxDistance, &OctreeNode::SetMaxDistance, 0.0f);
}

void OctreeNode::SetCastShadows(bool enable)
{
    if (TestFlag(NF_CAST_SHADOWS) != enable)
    {
        SetFlag(NF_CAST_SHADOWS, enable);
        // Reinsert into octree so that cached shadow map invalidation is handled
        OnTransformChanged();
    }
}

void OctreeNode::SetMaxDistance(float distance_)
{
    maxDistance = Max(distance_, 0.0f);
}

void OctreeNode::OnOctreeUpdate(unsigned short)
{
}

bool OctreeNode::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    distance = camera->Distance(WorldBoundingBox().Center());

    if (maxDistance > 0.0f && distance > maxDistance)
        return false;

    lastFrameNumber = frameNumber;
    return true;
}

void OctreeNode::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
{
    float hitDistance = ray.HitDistance(WorldBoundingBox());
    if (hitDistance < maxDistance_)
    {
        RaycastResult res;
        res.position = ray.origin + hitDistance * ray.direction;
        res.normal = -ray.direction;
        res.distance = hitDistance;
        res.node = this;
        res.subObject = 0;
        dest.push_back(res);
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
        if (octree && IsEnabled())
            octree->QueueUpdate(this);
    }
}

void OctreeNode::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();
    SetFlag(NF_BOUNDING_BOX_DIRTY, true);
    if (octree && (Flags() & (NF_OCTREE_REINSERT_QUEUED | NF_ENABLED)) == NF_ENABLED)
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

void OctreeNode::OnEnabledChanged(bool newEnabled)
{
    if (octree)
    {
        if (newEnabled)
            octree->QueueUpdate(this);
        else
            octree->RemoveNode(this);
    }
}