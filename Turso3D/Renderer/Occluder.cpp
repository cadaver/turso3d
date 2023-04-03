// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "Camera.h"
#include "DebugRenderer.h"
#include "Model.h"
#include "Octree.h"
#include "Occluder.h"

#include <tracy/Tracy.hpp>

static Allocator<OccluderDrawable> drawableAllocator;

void OccluderDrawable::OnWorldBoundingBoxUpdate() const
{
    if (model)
        worldBoundingBox = model->LocalBoundingBox().Transformed(WorldTransform());
    else
        Drawable::OnWorldBoundingBoxUpdate();
}

bool OccluderDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    distance = camera->Distance(WorldBoundingBox().Center());

    if (maxDistance > 0.0f && distance > maxDistance)
    {
        distance = M_MAX_FLOAT;
        return false;
    }

    lastFrameNumber = frameNumber;
    return true;
}

void OccluderDrawable::OnRenderDebug(DebugRenderer* debug)
{
    debug->AddBoundingBox(WorldBoundingBox(), Color::RED, false);
}

Occluder::Occluder()
{
    drawable = drawableAllocator.Allocate();
    drawable->SetOwner(this);
}

Occluder::~Occluder()
{
    if (drawable)
    {
        RemoveFromOctree();
        drawableAllocator.Free(static_cast<OccluderDrawable*>(drawable));
        drawable = nullptr;
    }
}

void Occluder::RegisterObject()
{
    RegisterFactory<Occluder>();
    CopyBaseAttributes<Occluder, SpatialNode>();
    RegisterDerivedType<Occluder, SpatialNode>();
    RegisterMixedRefAttribute("model", &Occluder::ModelAttr, &Occluder::SetModelAttr, ResourceRef(Model::TypeStatic()));
    RegisterAttribute("maxDistance", &Occluder::MaxDistance, &Occluder::SetMaxDistance, 0.0f);
}

void Occluder::SetModel(Model* model)
{
    ZoneScoped;

    OccluderDrawable* occluderDrawable = static_cast<OccluderDrawable*>(drawable);
    occluderDrawable->model = model;

    if (model)
    {
        occluderDrawable->batches.SetNumGeometries(model->NumGeometries());
        for (size_t i = 0; i < model->NumGeometries(); ++i)
        {
            // If model has multiple LODs, use the lowest for software occlusion
            occluderDrawable->batches.SetGeometry(i, model->GetGeometry(i, model->LodGeometries(i).size() - 1));
        }
    }
    else
    {
        occluderDrawable->batches.SetNumGeometries(0);
        if (octree)
            octree->RemoveOccluder(drawable);
    }

    OnBoundingBoxChanged();
}

Model* Occluder::GetModel() const
{
    return static_cast<OccluderDrawable*>(drawable)->model;
}

void Occluder::SetMaxDistance(float distance_)
{
    OccluderDrawable* occluderDrawable = static_cast<OccluderDrawable*>(drawable);
    occluderDrawable->maxDistance = Max(distance_, 0.0f);
}

void Occluder::OnSceneSet(Scene* newScene, Scene*)
{
    /// Remove from current octree if any
    RemoveFromOctree();

    if (newScene)
    {
        // Octree must be attached to the scene root as a child
        octree = newScene->FindChild<Octree>();
        OnBoundingBoxChanged();
    }
}

void Occluder::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();

    drawable->SetFlag(DF_WORLD_TRANSFORM_DIRTY | DF_BOUNDING_BOX_DIRTY, true);
    // Defer octree insertion until has the model set
    if (octree && IsEnabled() && static_cast<OccluderDrawable*>(drawable)->model)
        octree->InsertOccluder(drawable);
}

void Occluder::OnBoundingBoxChanged()
{
    drawable->SetFlag(DF_BOUNDING_BOX_DIRTY, true);
    if (octree && IsEnabled() && static_cast<OccluderDrawable*>(drawable)->model)
        octree->InsertOccluder(drawable);
}

void Occluder::RemoveFromOctree()
{
    if (octree)
    {
        octree->RemoveOccluder(drawable);
        octree = nullptr;
    }
}

void Occluder::OnEnabledChanged(bool newEnabled)
{
    if (octree)
    {
        if (newEnabled)
        {
            if (static_cast<OccluderDrawable*>(drawable)->model)
                octree->InsertOccluder(drawable);
        }
        else
            octree->RemoveOccluder(drawable);
    }
}

void Occluder::SetModelAttr(const ResourceRef& value)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    SetModel(cache->LoadResource<Model>(value.name));
}

ResourceRef Occluder::ModelAttr() const
{
    return ResourceRef(Model::TypeStatic(), ResourceName(GetModel()));
}
