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
    geomPtr = nullptr;
}

SourceBatches::~SourceBatches()
{
    if (numGeometries >= 2)
    {
        for (size_t i = 0; i < numGeometries; ++i)
            (reinterpret_cast<SharedPtr<Material>*>(geomPtr) + i * 2 + 1)->~SharedPtr<Material>();

        delete[] reinterpret_cast<Geometry**>(geomPtr);
    }
}

void SourceBatches::SetNumGeometries(size_t num)
{
    if (numGeometries == num)
        return;

    if (numGeometries < 2)
    {
        geomPtr = nullptr;
        matPtr.Reset();
    }
    else
    {
        for (size_t i = 0; i < numGeometries; ++i)
            (reinterpret_cast<SharedPtr<Material>*>(geomPtr) + i * 2 + 1)->~SharedPtr<Material>();

        delete[] reinterpret_cast<Geometry**>(geomPtr);
    }

    numGeometries = num;

    if (numGeometries < 2)
    {
        geomPtr = nullptr;
        matPtr.Reset();
    }
    else
    {
        geomPtr = new Geometry*[numGeometries * 2];
        for (size_t i = 0; i < numGeometries; ++i)
        {
            *(reinterpret_cast<Geometry**>(geomPtr) + i * 2) = nullptr;
            new (reinterpret_cast<SharedPtr<Material>*>(geomPtr) + i * 2 + 1) SharedPtr<Material>();
        }
    }

    for (size_t i = 0; i < numGeometries; ++i)
        SetMaterial(i, Material::DefaultMaterial());
}

Geometry::Geometry() :
    drawStart(0),
    drawCount(0),
    lodDistance(0.0f),
    cpuDrawStart(0),
    cpuIndexSize(0)
{
}

Geometry::~Geometry()
{
}

float Geometry::HitDistance(const Ray& ray, Vector3* outNormal) const
{
    if (!cpuPositionData)
        return M_INFINITY;
    
    if (cpuIndexData)
        return ray.HitDistance(cpuPositionData, sizeof(Vector3), cpuIndexData, cpuIndexSize, cpuDrawStart, drawCount, outNormal);
    else
        return ray.HitDistance(cpuPositionData, sizeof(Vector3), cpuDrawStart, drawCount, outNormal);
}

GeometryDrawable::GeometryDrawable()
{
    SetFlag(DF_GEOMETRY, true);
}

bool GeometryDrawable::OnPrepareRender(unsigned short frameNumber, Camera* camera)
{
    distance = camera->Distance(WorldPosition());

    if (maxDistance > 0.0f && distance > maxDistance)
        return false;

    lastFrameNumber = frameNumber;
    return true;
}

void GeometryDrawable::OnRender(ShaderProgram*, size_t)
{
}

void GeometryNode::RegisterObject()
{
    RegisterDerivedType<GeometryNode, OctreeNode>();
    CopyBaseAttributes<GeometryNode, OctreeNode>();
    RegisterMixedRefAttribute("materials", &GeometryNode::MaterialsAttr, &GeometryNode::SetMaterialsAttr,
        ResourceRefList(Material::TypeStatic()));
}

void GeometryNode::SetNumGeometries(size_t num)
{
    GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
    geomDrawable->batches.SetNumGeometries(num);
}

void GeometryNode::SetGeometry(size_t index, Geometry* geometry)
{
    if (!geometry)
    {
        LOGERROR("Can not assign null geometry");
        return;
    }

    GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
    if (index < geomDrawable->batches.NumGeometries())
        geomDrawable->batches.SetGeometry(index, geometry);
}

void GeometryNode::SetMaterial(Material* material)
{
    if (!material)
        material = Material::DefaultMaterial();

    GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
    for (size_t i = 0; i < geomDrawable->batches.NumGeometries(); ++i)
        geomDrawable->batches.SetMaterial(i, material);
}

void GeometryNode::SetMaterial(size_t index, Material* material)
{
    if (!material)
        material = Material::DefaultMaterial();

    GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
    if (index < geomDrawable->batches.NumGeometries())
        geomDrawable->batches.SetMaterial(index, material);
}

void GeometryNode::SetMaterialsAttr(const ResourceRefList& value)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    for (size_t i = 0; i < value.names.size(); ++i)
        SetMaterial(i, cache->LoadResource<Material>(value.names[i]));
}

ResourceRefList GeometryNode::MaterialsAttr() const
{
    ResourceRefList ret(Material::TypeStatic());
    
    GeometryDrawable* geomDrawable = static_cast<GeometryDrawable*>(drawable);
    for (size_t i = 0; i < geomDrawable->batches.NumGeometries(); ++i)
        ret.names.push_back(ResourceName(GetMaterial(i)));

    return ret;
}
