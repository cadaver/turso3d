// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "../Resource/ResourceCache.h"
#include "Camera.h"
#include "GeometryNode.h"
#include "Material.h"

SourceBatches::SourceBatches()
{
    numGeometries = 0;

    new (&geomPtr) SharedPtr<Geometry>();
    new (&matPtr) SharedPtr<Material>();
}

SourceBatches::~SourceBatches()
{
    if (numGeometries < 2)
    {
        reinterpret_cast<SharedPtr<Geometry>*>(&geomPtr)->~SharedPtr<Geometry>();
        reinterpret_cast<SharedPtr<Material>*>(&matPtr)->~SharedPtr<Material>();
    }
    else
    {
        for (size_t i = 0; i < numGeometries; ++i)
        {
            reinterpret_cast<SharedPtr<Geometry>*>(geomPtr + i * 2)->~SharedPtr<Geometry>();
            reinterpret_cast<SharedPtr<Material>*>(geomPtr + i * 2 + 1)->~SharedPtr<Material>();
        }

        delete[] geomPtr;
    }
}

void SourceBatches::SetNumGeometries(size_t num)
{
    if (numGeometries == num)
        return;

    if (numGeometries < 2 && num < 2)
    {
        numGeometries = num;
        for (size_t i = 0; i < numGeometries; ++i)
            SetMaterial(i, Material::DefaultMaterial());
        return;
    }

    if (numGeometries < 2)
    {
        reinterpret_cast<SharedPtr<Geometry>*>(&geomPtr)->~SharedPtr<Geometry>();
        reinterpret_cast<SharedPtr<Material>*>(&matPtr)->~SharedPtr<Material>();
    }
    else
    {
        for (size_t i = 0; i < numGeometries; ++i)
        {
            reinterpret_cast<SharedPtr<Geometry>*>(geomPtr + i * 2)->~SharedPtr<Geometry>();
            reinterpret_cast<SharedPtr<Material>*>(geomPtr + i * 2 + 1)->~SharedPtr<Material>();
        }

        delete[] geomPtr;
    }

    numGeometries = num;

    if (numGeometries < 2)
    {
        new (&geomPtr) SharedPtr<Geometry>();
        new (&matPtr) SharedPtr<Material>();
    }
    else
    {
        geomPtr = new size_t[numGeometries * 2];
        for (size_t i = 0; i < numGeometries; ++i)
        {
            new (geomPtr + i * 2) SharedPtr<Geometry>();
            new (geomPtr + i * 2 + 1) SharedPtr<Material>();
        }
    }

    for (size_t i = 0; i < numGeometries; ++i)
        SetMaterial(i, Material::DefaultMaterial());
}

Geometry::Geometry() : 
    drawStart(0),
    drawCount(0),
    lodDistance(0.0f),
    useCombined(false)
{
}

Geometry::~Geometry()
{
}

GeometryNode::GeometryNode() :
    lightList(nullptr)
{
    SetFlag(NF_GEOMETRY, true);
}

GeometryNode::~GeometryNode()
{
}

void GeometryNode::RegisterObject()
{
    RegisterFactory<GeometryNode>();
    RegisterDerivedType<GeometryNode, OctreeNode>();
    CopyBaseAttributes<GeometryNode, OctreeNode>();
    RegisterMixedRefAttribute("materials", &GeometryNode::MaterialsAttr, &GeometryNode::SetMaterialsAttr,
        ResourceRefList(Material::TypeStatic()));
}

bool GeometryNode::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    distance = camera->Distance(WorldPosition());
    lightList = nullptr;

    if (maxDistance > 0.0f && distance > maxDistance)
        return false;

    lastFrameNumber = frameNumber;
    return true;
}

void GeometryNode::SetNumGeometries(size_t num)
{
    batches.SetNumGeometries(num);
}

void GeometryNode::SetGeometry(size_t index, Geometry* geometry)
{
    if (!geometry)
    {
        LOGERROR("Can not assign null geometry");
        return;
    }

    if (index < batches.NumGeometries())
        batches.SetGeometry(index, geometry);
}

void GeometryNode::SetMaterial(Material* material)
{
    if (!material)
        material = Material::DefaultMaterial();

    for (size_t i = 0; i < batches.NumGeometries(); ++i)
        batches.SetMaterial(i, material);
}

void GeometryNode::SetMaterial(size_t index, Material* material)
{
    if (!material)
        material = Material::DefaultMaterial();

    if (index < batches.NumGeometries())
        batches.SetMaterial(index, material);
}

void GeometryNode::SetMaterialsAttr(const ResourceRefList& materials)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    for (size_t i = 0; i < materials.names.size(); ++i)
        SetMaterial(i, cache->LoadResource<Material>(materials.names[i]));
}

ResourceRefList GeometryNode::MaterialsAttr() const
{
    ResourceRefList ret(Material::TypeStatic());
    
    for (size_t i = 0; i < batches.NumGeometries(); ++i)
        ret.names[i] = ResourceName(GetMaterial(i));

    return ret;
}
