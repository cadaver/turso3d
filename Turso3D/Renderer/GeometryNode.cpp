// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "../Resource/ResourceCache.h"
#include "Camera.h"
#include "GeometryNode.h"
#include "Material.h"
#include "Renderer.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Geometry::Geometry() : 
    primitiveType(TRIANGLE_LIST),
    drawStart(0),
    drawCount(0),
    lodDistance(0.0f)
{
}

Geometry::~Geometry()
{
}

SourceBatch::SourceBatch()
{
}

SourceBatch::~SourceBatch()
{
}

GeometryNode::GeometryNode() :
    lightList(nullptr),
    lastFrameNumber(0),
    geometryType(GEOM_STATIC),
    squaredDistance(0.0f)
{
    SetFlag(NF_GEOMETRY, true);
}

GeometryNode::~GeometryNode()
{
}

void GeometryNode::RegisterObject()
{
    RegisterFactory<GeometryNode>();
    CopyBaseAttributes<GeometryNode, SpatialNode>();
    RegisterMixedRefAttribute("materials", &GeometryNode::MaterialsAttr, &GeometryNode::SetMaterialsAttr,
        ResourceRefList(Material::TypeStatic()));
}

void GeometryNode::OnPrepareRender(Camera* camera)
{
    squaredDistance = camera->SquaredDistance(WorldPosition());
}

void GeometryNode::SetGeometryType(GeometryType type)
{
    geometryType = type;
}

void GeometryNode::SetNumGeometries(size_t num)
{
    batches.Resize(num);
    
    // Ensure non-null materials
    for (auto it = batches.Begin(); it != batches.End(); ++it)
    {
        if (!it->material.Get())
            it->material = Material::DefaultMaterial();
    }
}

void GeometryNode::SetGeometry(size_t index, Geometry* geometry)
{
    if (!geometry)
    {
        LOGERROR("Can not assign null geometry");
        return;
    }

    if (index < batches.Size())
        batches[index].geometry = geometry;
    else
        LOGERRORF("Out of bounds batch index %d for setting geometry", (int)index);
}

void GeometryNode::SetMaterial(size_t index, Material* material)
{
    if (index < batches.Size())
    {
        if (!material)
            material = Material::DefaultMaterial();
        batches[index].material = material;
    }
    else
        LOGERRORF("Out of bounds batch index %d for setting material", (int)index);
}

void GeometryNode::SetLocalBoundingBox(const BoundingBox& box)
{
    boundingBox = box;
    // Changing the bounding box may require octree reinsertion
    OctreeNode::OnTransformChanged();
}

Geometry* GeometryNode::GetGeometry(size_t index) const
{
    return index < batches.Size() ? batches[index].geometry.Get() : nullptr;
}

Material* GeometryNode::GetMaterial(size_t index) const
{
    return index < batches.Size() ? batches[index].material.Get() : nullptr;
}

void GeometryNode::OnWorldBoundingBoxUpdate() const
{
    worldBoundingBox = boundingBox.Transformed(WorldTransform());
    SetFlag(NF_BOUNDING_BOX_DIRTY, false);
}

void GeometryNode::SetMaterialsAttr(const ResourceRefList& materials)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    for (size_t i = 0; i < materials.names.Size(); ++i)
        SetMaterial(i, cache->LoadResource<Material>(materials.names[i]));
}

ResourceRefList GeometryNode::MaterialsAttr() const
{
    ResourceRefList ret(Material::TypeStatic());
    
    ret.names.Resize(batches.Size());
    for (size_t i = 0; i < batches.Size(); ++i)
        ret.names[i] = ResourceName(batches[i].material.Get());

    return ret;
}

}
