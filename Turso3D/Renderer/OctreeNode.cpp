// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/Ray.h"
#include "../Scene/Scene.h"
#include "Camera.h"
#include "DebugRenderer.h"
#include "Octree.h"
#include "OctreeNode.h"

Drawable::Drawable() :
    owner(nullptr),
    octant(nullptr),
    flags(0),
    layer(LAYER_DEFAULT),
    lastFrameNumber(0),
    lastUpdateFrameNumber(0),
    distance(0.0f),
    maxDistance(0.0f)
{
    SetFlag(DF_BOUNDING_BOX_DIRTY, true);
}

Drawable::~Drawable()
{
}

void Drawable::OnWorldBoundingBoxUpdate() const
{
    // The Drawable base class does not have a defined size, so represent as a point
    worldBoundingBox.Define(WorldPosition());
    SetFlag(DF_BOUNDING_BOX_DIRTY, false);
}

void Drawable::OnOctreeUpdate(unsigned short)
{
}

bool Drawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    distance = camera->Distance(WorldBoundingBox().Center());

    if (maxDistance > 0.0f && distance > maxDistance)
        return false;

    lastFrameNumber = frameNumber;
    return true;
}

void Drawable::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
{
    float hitDistance = ray.HitDistance(WorldBoundingBox());
    if (hitDistance < maxDistance_)
    {
        RaycastResult res;
        res.position = ray.origin + hitDistance * ray.direction;
        res.normal = -ray.direction;
        res.distance = hitDistance;
        res.drawable = this;
        res.node = owner;
        res.subObject = 0;
        dest.push_back(res);
    }
}

void Drawable::OnRenderDebug(DebugRenderer* debug)
{
    debug->AddBoundingBox(WorldBoundingBox(), Color::GREEN, false);
}

void Drawable::SetOwner(OctreeNode* owner_)
{
    owner = owner_;
    worldTransform = const_cast<Matrix3x4*>(&owner_->WorldTransform());
}


OctreeNode::OctreeNode() :
    octree(nullptr),
    drawable(nullptr)
{
}

void OctreeNode::RegisterObject()
{
    CopyBaseAttributes<OctreeNode, SpatialNode>();
    RegisterDerivedType<OctreeNode, SpatialNode>();
    RegisterAttribute("static", &OctreeNode::IsStatic, &OctreeNode::SetStatic, false);
    RegisterAttribute("castShadows", &OctreeNode::CastShadows, &OctreeNode::SetCastShadows, false);
    RegisterAttribute("maxDistance", &OctreeNode::MaxDistance, &OctreeNode::SetMaxDistance, 0.0f);
}

void OctreeNode::SetStatic(bool enable)
{
    if (enable != IsStatic())
    {
        drawable->SetFlag(DF_STATIC, enable);
        // Handle possible octree reinsertion
        OnTransformChanged();
    }
}

void OctreeNode::SetCastShadows(bool enable)
{
    if (drawable->TestFlag(DF_CAST_SHADOWS) != enable)
    {
        drawable->SetFlag(DF_CAST_SHADOWS, enable);
        // Reinsert into octree so that cached shadow map invalidation is handled
        OnTransformChanged();
    }
}

void OctreeNode::SetMaxDistance(float distance_)
{
    drawable->maxDistance = Max(distance_, 0.0f);
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
            octree->QueueUpdate(drawable);
    }
}

void OctreeNode::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();
    drawable->SetFlag(DF_WORLD_TRANSFORM_DIRTY | DF_BOUNDING_BOX_DIRTY, true);

    if (drawable->octant && !drawable->TestFlag(DF_OCTREE_REINSERT_QUEUED))
        octree->QueueUpdate(drawable);
}

void OctreeNode::RemoveFromOctree()
{
    if (octree)
    {
        octree->RemoveDrawable(drawable);
        octree = nullptr;
    }
}

void OctreeNode::OnEnabledChanged(bool newEnabled)
{
    if (octree)
    {
        if (newEnabled)
            octree->QueueUpdate(drawable);
        else
            octree->RemoveDrawable(drawable);
    }
}

void OctreeNode::OnLayerChanged(unsigned char newLayer)
{
    drawable->layer = newLayer;
}
