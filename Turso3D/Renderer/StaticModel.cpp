// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "Camera.h"
#include "Model.h"
#include "Octree.h"
#include "StaticModel.h"

#include <tracy/Tracy.hpp>

static Vector3 DOT_SCALE(1 / 3.0f, 1 / 3.0f, 1 / 3.0f);

static Allocator<StaticModelDrawable> drawableAllocator;

StaticModelDrawable::StaticModelDrawable() :
    lodBias(1.0f)
{
}

void StaticModelDrawable::OnWorldBoundingBoxUpdate() const
{
    if (model)
        worldBoundingBox = model->LocalBoundingBox().Transformed(WorldTransform());
    else
        Drawable::OnWorldBoundingBoxUpdate();
}

bool StaticModelDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    distance = camera->Distance(WorldBoundingBox().Center());

    if (maxDistance > 0.0f && distance > maxDistance)
        return false;

    lastFrameNumber = frameNumber;

    // If model was last updated long ago, reset update framenumber to illegal
    if (frameNumber - lastUpdateFrameNumber == 0x8000)
        lastUpdateFrameNumber = 0;

    // Find out the new LOD level if model has LODs
    if (Flags() & DF_HAS_LOD_LEVELS)
    {
        float lodDistance = camera->LodDistance(distance, WorldScale().DotProduct(DOT_SCALE), lodBias);
        size_t numGeometries = batches.NumGeometries();

        for (size_t i = 0; i < numGeometries; ++i)
        {
            const std::vector<SharedPtr<Geometry> >& lodGeometries = model->LodGeometries(i);
            if (lodGeometries.size() > 1)
            {
                size_t j;
                for (j = 1; j < lodGeometries.size(); ++j)
                {
                    if (lodDistance <= lodGeometries[j]->lodDistance)
                        break;
                }
                if (batches.GetGeometry(i) != lodGeometries[j - 1])
                {
                    batches.SetGeometry(i, lodGeometries[j - 1]);
                    lastUpdateFrameNumber = frameNumber;
                }
            }
        }
    }

    return true;
}

void StaticModelDrawable::OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance_)
{
    if (ray.HitDistance(WorldBoundingBox()) < maxDistance_)
    {
        RaycastResult res;
        res.distance = M_INFINITY;

        // Perform model raycast in its local space
        const Matrix3x4& transform = WorldTransform();
        Ray localRay = ray.Transformed(transform.Inverse());

        size_t numGeometries = batches.NumGeometries();

        for (size_t i = 0; i < numGeometries; ++i)
        {
            Geometry* geom = batches.GetGeometry(i);
            float localDistance = geom->HitDistance(localRay, &res.normal);

            if (localDistance < M_INFINITY)
            {
                // If has a hit, transform it back to world space
                Vector3 hitPosition = transform * (localRay.origin + localDistance * localRay.direction);
                float hitDistance = (hitPosition - ray.origin).Length();

                if (hitDistance < maxDistance_ && hitDistance < res.distance)
                {
                    res.position = hitPosition;
                    res.normal = (transform * Vector4(res.normal, 0.0f)).Normalized();
                    res.distance = hitDistance;
                    res.drawable = this;
                    res.subObject = i;
                }
            }
        }

        if (res.distance < maxDistance_)
            dest.push_back(res);
    }
}

StaticModel::StaticModel()
{
    drawable = drawableAllocator.Allocate();
    drawable->SetOwner(this);
}

StaticModel::~StaticModel()
{
    if (drawable)
    {
        RemoveFromOctree();
        drawableAllocator.Free(static_cast<StaticModelDrawable*>(drawable));
        drawable = nullptr;
    }
}

void StaticModel::RegisterObject()
{
    RegisterFactory<StaticModel>();
    // Copy base attributes from OctreeNode instead of GeometryNode, as the model attribute needs to be set first so that there is the correct amount of materials to assign
    CopyBaseAttributes<StaticModel, OctreeNode>();
    RegisterDerivedType<StaticModel, GeometryNode>();
    RegisterMixedRefAttribute("model", &StaticModel::ModelAttr, &StaticModel::SetModelAttr, ResourceRef(Model::TypeStatic()));
    CopyBaseAttribute<StaticModel, GeometryNode>("materials");
    RegisterAttribute("lodBias", &StaticModel::LodBias, &StaticModel::SetLodBias, 1.0f);
}

void StaticModel::SetModel(Model* model)
{
    ZoneScoped;

    StaticModelDrawable* modelDrawable = static_cast<StaticModelDrawable*>(drawable);

    modelDrawable->model = model;
    modelDrawable->SetFlag(DF_HAS_LOD_LEVELS, false);

    if (model)
    {
        SetNumGeometries(model->NumGeometries());
        // Start at LOD level 0
        for (size_t i = 0; i < model->NumGeometries(); ++i)
        {
            SetGeometry(i, model->GetGeometry(i, 0));
            if (model->NumLodLevels(i) > 1)
                modelDrawable->SetFlag(DF_HAS_LOD_LEVELS, true);
        }
    }
    else
    {
        SetNumGeometries(0);
    }

    OnBoundingBoxChanged();
}

void StaticModel::SetLodBias(float bias)
{
    StaticModelDrawable* modelDrawable = static_cast<StaticModelDrawable*>(drawable);
    modelDrawable->lodBias = Max(bias, M_EPSILON);
}

Model* StaticModel::GetModel() const
{
    return static_cast<StaticModelDrawable*>(drawable)->model;
}

void StaticModel::SetModelAttr(const ResourceRef& value)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    SetModel(cache->LoadResource<Model>(value.name));
}

ResourceRef StaticModel::ModelAttr() const
{
    return ResourceRef(Model::TypeStatic(), ResourceName(GetModel()));
}
