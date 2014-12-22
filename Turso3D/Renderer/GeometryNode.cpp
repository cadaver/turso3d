// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "GeometryNode.h"
#include "Material.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

SourceBatch::SourceBatch() :
    geometryType(GEOM_STATIC),
    primitiveType(TRIANGLE_LIST),
    drawStart(0),
    drawCount(0),
    worldMatrix(nullptr)
{
}

SourceBatch::~SourceBatch()
{
}

GeometryNode::GeometryNode()
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
    float distance = camera->Distance(WorldPosition());

    for (auto it = batches.Begin(); it != batches.End(); ++it)
        it->distance = distance;
}

void GeometryNode::SetupBatches(GeometryType type, size_t numBatches)
{
    batches.Resize(numBatches);

    for (auto it = batches.Begin(); it != batches.End(); ++it)
    {
        it->geometryType = type;
        it->worldMatrix = &WorldTransform();
    }
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

Material* GeometryNode::GetMaterial(size_t index) const
{
    return index < batches.Size() ? batches[index].material : nullptr;
}

void GeometryNode::OnWorldBoundingBoxUpdate() const
{
    worldBoundingBox = boundingBox.Transformed(WorldTransform());
    SetFlag(NF_BOUNDING_BOX_DIRTY, false);
}

}
