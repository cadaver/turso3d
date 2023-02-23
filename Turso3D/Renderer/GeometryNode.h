// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Graphics/GraphicsDefs.h"
#include "../IO/ResourceRef.h"
#include "OctreeNode.h"

class GeometryNode;
class IndexBuffer;
class Material;
class Pass;
class ShaderProgram;
class VertexBuffer;

/// Description of geometry to be rendered. %Scene nodes that render the same object can share these to reduce memory load and allow instancing.
struct Geometry : public RefCounted
{
    /// Construct.
    Geometry();
    /// Destruct.
    ~Geometry();

    /// Last sort key for combined distance and state sorting. Used by Renderer.
    std::pair<unsigned short, unsigned short> lastSortKey;

    /// %Geometry vertex buffer.
    SharedPtr<VertexBuffer> vertexBuffer;
    /// %Geometry index buffer.
    SharedPtr<IndexBuffer> indexBuffer;
    /// Draw range start. Specifies index start if index buffer defined, vertex start otherwise.
    size_t drawStart;
    /// Draw range count. Specifies number of indices if index buffer defined, number of vertices otherwise.
    size_t drawCount;
    /// LOD transition distance.
    float lodDistance;
};

/// Draw call source data with optimal memory storage.
class SourceBatches
{
public:
    /// Construct.
    SourceBatches();
    /// Destruct.
    ~SourceBatches();

    /// Set number of geometries. Will destroy previously set geometry and material assignments.
    void SetNumGeometries(size_t num);

    /// Set geometry at index.
    void SetGeometry(size_t index, Geometry* geometry)
    {
        if (numGeometries < 2)
            *(reinterpret_cast<Geometry**>(&geomPtr)) = geometry;
        else
            *(reinterpret_cast<Geometry**>(geomPtr + index * 2)) = geometry;
    } 

    /// Set material at index.
    void SetMaterial(size_t index, Material* material)
    {
        if (numGeometries < 2)
            *reinterpret_cast<SharedPtr<Material>*>(&matPtr) = material;
        else
            *reinterpret_cast<SharedPtr<Material>*>(geomPtr + index * 2 + 1) = material;
    }

    /// Get number of geometries.
    size_t NumGeometries() const { return numGeometries; }

    /// Get geometry at index.
    Geometry* GetGeometry(size_t index) const
    {
        if (numGeometries < 2)
            return reinterpret_cast<Geometry*>(geomPtr);
        else
            return *(reinterpret_cast<Geometry**>(geomPtr + index * 2));
    }

    /// Get material at index.
    Material* GetMaterial(size_t index) const
    {
        if (numGeometries < 2)
            return reinterpret_cast<const SharedPtr<Material>*>(&matPtr)->Get();
        else
            return reinterpret_cast<const SharedPtr<Material>*>(geomPtr + index * 2 + 1)->Get();
    }

private:
    /// Geometry pointer or dynamic storage.
    size_t* geomPtr;
    /// Material pointer.
    size_t* matPtr;
    /// Number of geometries.
    size_t numGeometries;
};

/// Base class for scene nodes that contain geometry to be rendered.
class GeometryNode : public OctreeNode
{
    OBJECT(GeometryNode);

public:
    /// Construct.
    GeometryNode();
    /// Destruct.
    ~GeometryNode();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Prepare object for rendering. Reset framenumber and calculate distance from camera. Called by Renderer in worker threads. Return false if should not render.
    bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
    /// Update GPU resources and set uniforms for rendering. Called by Renderer when geometry type is not static.
    virtual void OnRender(size_t geomIndex, ShaderProgram* program);

    /// Set number of geometries.
    void SetNumGeometries(size_t num);
    /// Set geometry at index.
    void SetGeometry(size_t index, Geometry* geometry);
    /// Set material at every geometry index. Specifying null will use the default material (opaque white.)
    void SetMaterial(Material* material);
    /// Set material at geometry index.
    void SetMaterial(size_t index, Material* material);

    /// Return geometry type.
    GeometryType GetGeometryType() const { return (GeometryType)(Flags() >> 14); }
    /// Return number of geometries / batches.
    size_t NumGeometries() const { return batches.NumGeometries(); }
    /// Return geometry by index.
    Geometry* GetGeometry(size_t index) const { return batches.GetGeometry(index); }
    /// Return material by geometry index.
    Material* GetMaterial(size_t index) const { return batches.GetMaterial(index); }
    /// Return the draw call source data for direct access
    const SourceBatches& Batches() const { return batches; }

protected:
    /// Set materials list. Used in serialization.
    void SetMaterialsAttr(const ResourceRefList& value);
    /// Return materials list. Used in serialization.
    ResourceRefList MaterialsAttr() const;

    /// Draw call source data.
    SourceBatches batches;
};
