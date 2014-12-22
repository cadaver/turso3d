// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Graphics/GraphicsDefs.h"
#include "OctreeNode.h"

namespace Turso3D
{

class ConstantBuffer;
class IndexBuffer;
class Material;
class VertexBuffer;

/// Geometry types.
enum GeometryType
{
    GEOM_STATIC = 0,
    GEOM_INSTANCED
};

/// Description of a geometry node's draw call.
struct SourceBatch
{
    /// Construct with defaults.
    SourceBatch();

    /// Destruct
    ~SourceBatch();

    /// Geometry vertex buffer.
    SharedPtr<VertexBuffer> vertexBuffer;
    /// Geometry index buffer.
    SharedPtr<IndexBuffer> indexBuffer;
    /// Material resource.
    SharedPtr<Material> material;
    /// Object constant buffers. Leaving these null and using the STATIC geometry type allows to use instancing.
    SharedPtr<ConstantBuffer> constantBuffers[MAX_SHADER_STAGES];
    /// Geometry type. Affects the vertex shader variation that will be chosen.
    GeometryType geometryType;
    /// Geometry's primitive type.
    PrimitiveType primitiveType;
    /// Draw range start. Specifies index start if index buffer defined, vertex start otherwise.
    size_t drawStart;
    /// Draw range count. Specifies number of indices if index buffer defined, number of vertices otherwise.
    size_t drawCount;
    /// Pointer to the world matrix.
    const Matrix3x4* worldMatrix;
    /// Distance of batch from last camera rendered from.
    float distance;
};

/// Base class for nodes that contain geometry to be rendered.
class GeometryNode : public OctreeNode
{
public:
    /// Construct.
    GeometryNode();
    /// Destruct.
    ~GeometryNode();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Prepare object for rendering. Called by Renderer.
    virtual void OnPrepareRender(Camera* camera);

    /// Set geometry type and number of batches. Assigns the batches to use the node's world matrix.
    void SetupBatches(GeometryType type, size_t numBatches);
    /// Set material in batch.
    void SetMaterial(size_t index, Material* material);
    /// Set local space bounding box.
    void SetBoundingBox(const BoundingBox& box);
    /// Return modifiable batch by index.
    SourceBatch* GetBatch(size_t index) { return index < batches.Size() ? &batches[index] : nullptr; }
    /// Return modifiable batches.
    Vector<SourceBatch>& Batches() { return batches; }

    /// Return material in batch.
    Material* GetMaterial(size_t index) const;
    /// Return local space bounding box.
    const BoundingBox& GetBoundingBox() const { return boundingBox; }
    /// Return const batch by index.
    const SourceBatch* GetBatch(size_t index) const { return index < batches.Size() ? &batches[index] : nullptr; }
    /// Return const batches.
    const Vector<SourceBatch>& Batches() const { return batches; }

protected:
    /// Recalculate the world bounding box.
    void OnWorldBoundingBoxUpdate() const override;

    /// Batch data.
    Vector<SourceBatch> batches;
    /// Local space bounding box.
    BoundingBox boundingBox;
};

}
