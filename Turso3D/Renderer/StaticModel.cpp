// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Debug/Log.h"
#include "../Resource/ResourceCache.h"
#include "Camera.h"
#include "Material.h"
#include "Model.h"
#include "StaticModel.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static Vector3 DOT_SCALE(1 / 3.0f, 1 / 3.0f, 1 / 3.0f);

StaticModel::StaticModel() :
    lodBias(1.0f),
    hasLodLevels(false)
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
    RegisterMixedRefAttribute("model", &StaticModel::ModelAttr, &StaticModel::SetModelAttr, ResourceRef(Model::TypeStatic()));
    RegisterMixedRefAttribute("materials", &GeometryNode::MaterialsAttr, &GeometryNode::SetMaterialsAttr,
        ResourceRefList(Material::TypeStatic()));
    RegisterAttribute("lodBias", &StaticModel::LodBias, &StaticModel::SetLodBias, 1.0f);
}

void StaticModel::OnPrepareRender(Camera* camera)
{
    squaredDistance = camera->SquaredDistance(WorldPosition());

    // Find out the new LOD level if model has LODs
    if (hasLodLevels)
    {
        float lodDistance = camera->LodDistance(sqrtf(squaredDistance), WorldScale().DotProduct(DOT_SCALE), lodBias);

        for (size_t i = 0; i < batches.Size(); ++i)
        {
            const Vector<SharedPtr<Geometry> >& lodGeometries = model->LodGeometries(i);
            if (lodGeometries.Size() > 1)
            {
                size_t j;
                for (j = 1; j < lodGeometries.Size(); ++j)
                {
                    if (lodDistance <= lodGeometries[j]->lodDistance)
                        break;
                }
                batches[i].geometry = lodGeometries[j - 1];
            }
        }
    }
}

void StaticModel::SetModel(Model* model_)
{
    model = model_;
    hasLodLevels = false;

    if (!model)
    {
        SetNumGeometries(0);
        SetLocalBoundingBox(BoundingBox(0.0f, 0.0f));
        return;
    }

    SetNumGeometries(model->NumGeometries());
    // Start at LOD level 0
    for (size_t i = 0; i < batches.Size(); ++i)
    {
        SetGeometry(i, model->GetGeometry(i, 0));
        if (model->NumLodLevels(i) > 1)
            hasLodLevels = true;
    }

    SetLocalBoundingBox(model->LocalBoundingBox());
}

void StaticModel::SetLodBias(float bias)
{
    lodBias = Max(bias, M_EPSILON);
}

Model* StaticModel::GetModel() const
{
    return model.Get();
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

}
