// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "Camera.h"
#include "GeometryNode.h"
#include "Material.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Geometry::Geometry() : 
    primitiveType(TRIANGLE_LIST),
    drawStart(0),
    drawCount(0)
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
    geometryType(GEOM_STATIC),
    distance(0.0f)
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
}

void GeometryNode::OnPrepareRender(Camera* camera)
{
    distance = camera->Distance(WorldPosition());
}

void GeometryNode::SetGeometryType(GeometryType type)
{
    geometryType = type;
}

void GeometryNode::SetNumGeometries(size_t num)
{
    batches.Resize(num);
}

void GeometryNode::SetGeometry(size_t index, Geometry* geometry)
{
    if (index < batches.Size())
        batches[index].geometry = geometry;
    else
        LOGERRORF("Out of bounds batch index %d for setting geometry", (int)index);
}

void GeometryNode::SetMaterial(size_t index, Material* material)
{
    if (index < batches.Size())
        batches[index].material = material;
    else
        LOGERRORF("Out of bounds batch index %d for setting material", (int)index);
}

void GeometryNode::SetBoundingBox(const BoundingBox& box)
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

}
