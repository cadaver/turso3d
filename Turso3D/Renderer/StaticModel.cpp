// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "Camera.h"
#include "Material.h"
#include "Model.h"
#include "StaticModel.h"

static Vector3 DOT_SCALE(1 / 3.0f, 1 / 3.0f, 1 / 3.0f);

StaticModel::StaticModel() :
    lodBias(1.0f)
{
}

StaticModel::~StaticModel()
{
}

void StaticModel::RegisterObject()
{
    RegisterFactory<StaticModel>();
    // Copy base attributes from OctreeNode instead of GeometryNode, as the model attribute needs to be set first so that
    // there is the correct amount of materials to assign
    CopyBaseAttributes<StaticModel, OctreeNode>();
    RegisterDerivedType<StaticModel, GeometryNode>();
    RegisterMixedRefAttribute("model", &StaticModel::ModelAttr, &StaticModel::SetModelAttr, ResourceRef(Model::TypeStatic()));
    CopyBaseAttribute<StaticModel, GeometryNode>("materials");
    RegisterAttribute("lodBias", &StaticModel::LodBias, &StaticModel::SetLodBias, 1.0f);
}

bool StaticModel::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    distance = camera->Distance(WorldBoundingBox().Center());

    if (maxDistance > 0.0f && distance > maxDistance)
        return false;

    lastFrameNumber = frameNumber;

    // If model was last updated long ago, reset update framenumber to illegal
    if (frameNumber - lastUpdateFrameNumber == 0x8000)
        lastUpdateFrameNumber = 0;

    // Find out the new LOD level if model has LODs
    if (Flags() & NF_HASLODLEVELS)
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

void StaticModel::SetModel(Model* model_)
{
    model = model_;
    SetFlag(NF_HASLODLEVELS, false);

    if (model)
    {
        SetNumGeometries(model->NumGeometries());
        // Start at LOD level 0
        for (size_t i = 0; i < model->NumGeometries(); ++i)
        {
            SetGeometry(i, model->GetGeometry(i, 0));
            if (model->NumLodLevels(i) > 1)
                SetFlag(NF_HASLODLEVELS, true);
        }
    }
    else
    {
        SetNumGeometries(0);
    }

    OnTransformChanged();
}

void StaticModel::SetLodBias(float bias)
{
    lodBias = Max(bias, M_EPSILON);
}

Model* StaticModel::GetModel() const
{
    return model.Get();
}

void StaticModel::OnWorldBoundingBoxUpdate() const
{
    if (model)
    {
        worldBoundingBox = model->LocalBoundingBox().Transformed(WorldTransform());
        SetFlag(NF_BOUNDING_BOX_DIRTY, false);
    }
    else
        OctreeNode::OnWorldBoundingBoxUpdate();
}

void StaticModel::SetModelAttr(const ResourceRef& model_)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    SetModel(cache->LoadResource<Model>(model_.name));
}

ResourceRef StaticModel::ModelAttr() const
{
    return ResourceRef(Model::TypeStatic(), ResourceName(model.Get()));
}
