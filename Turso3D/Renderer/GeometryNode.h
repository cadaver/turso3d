// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Graphics/GraphicsDefs.h"
#include "../IO/ResourceRef.h"
#include "OctreeNode.h"

namespace Turso3D
{

class Camera;
class ConstantBuffer;
class IndexBuffer;
class Material;
class VertexBuffer;
struct LightList;

/// Geometry types.
enum GeometryType
{
    GEOM_STATIC = 0,
    GEOM_INSTANCED
};

/// Description of geometry to be rendered. %Scene nodes that render the same object can share these to reduce memory load and allow instancing.
struct TURSO3D_API Geometry : public RefCounted
{
    /// Default-construct.
    Geometry();
    /// Destruct.
    ~Geometry();

    /// %Geometry vertex buffer.
    SharedPtr<VertexBuffer> vertexBuffer;
    /// %Geometry index buffer.
    SharedPtr<IndexBuffer> indexBuffer;
    /// Constant buffers.
    SharedPtr<ConstantBuffer> constantBuffers[MAX_SHADER_STAGES];
    /// %Geometry's primitive type.
    PrimitiveType primitiveType;
    /// Draw range start. Specifies index start if index buffer defined, vertex start otherwise.
    size_t drawStart;
    /// Draw range count. Specifies number of indices if index buffer defined, number of vertices otherwise.
    size_t drawCount;
    /// LOD transition distance.
    float lodDistance;
};

/// Draw call source data.
struct TURSO3D_API SourceBatch
{
    /// Construct empty.
    SourceBatch();
    /// Destruct.
    ~SourceBatch();

    /// The geometry to render.
    SharedPtr<Geometry> geometry;
    /// The material to use for rendering.
    SharedPtr<Material> material;
};

/// Base class for scene nodes that contain geometry to be rendered.
class TURSO3D_API GeometryNode : public OctreeNode
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

    /// Set geometry type in all batches.
    void SetGeometryType(GeometryType type);
    /// Set number of geometries / batches.
    void SetNumGeometries(size_t num);
    /// Set geometry in batch.
    void SetGeometry(size_t index, Geometry* geometry);
    /// Set material in batch.
    void SetMaterial(size_t index, Material* material);
    /// Set local space bounding box.
    void SetLocalBoundingBox(const BoundingBox& box);

    /// Return geometry type.
    GeometryType GetGeometryType() const { return geometryType; }
    /// Return number of geometries.
    size_t NumGeometries() const { return batches.Size(); }
    /// Return geometry by batch index.
    Geometry* GetGeometry(size_t index) const;
    /// Return material by batch index.
    Material* GetMaterial(size_t index) const;
    /// Return source information for all batches.
    const Vector<SourceBatch>& Batches() const { return batches; }
    /// Return local space bounding box.
    const BoundingBox& LocalBoundingBox() const { return boundingBox; }
    /// Return squared distance from camera in the current view.
    float SquaredDistance() const { return squaredDistance; }

    /// %Light list for rendering. Assigned by Renderer.
    LightList* lightList;
    /// Last frame number of light list modification. Used to detect unlit objects without an extra pass through all geometries.
    unsigned lastFrameNumber;

protected:
    /// Recalculate the world space bounding box.
    void OnWorldBoundingBoxUpdate() const override;

    /// Set materials list. Used in serialization.
    void SetMaterialsAttr(const ResourceRefList& materials);
    /// Return materials list. Used in serialization.
    ResourceRefList MaterialsAttr() const;

    /// Geometry type.
    GeometryType geometryType;
    /// Draw call source datas.
    Vector<SourceBatch> batches;
    /// Local space bounding box.
    BoundingBox boundingBox;
    /// Squared distance from camera in the current view.
    /// \todo Should be per-batch to allow correct distance sorting of alpha-blended submeshes
    float squaredDistance;
};

}
