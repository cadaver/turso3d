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

    /// Return ray hit distance if has CPU-side data, or infinity if no hit or no data.
    float HitDistance(const Ray& ray, Vector3* outNormal = nullptr) const;

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
    /// Optional CPU-side position data.
    SharedArrayPtr<Vector3> cpuPositionData;
    /// Optional CPU-side index data.
    SharedArrayPtr<unsigned char> cpuIndexData;
    /// Optional index size for the CPU data. May be different in case combined vertex and index buffers are in use.
    size_t cpuIndexSize;
    /// Optional draw range start for the CPU data. May be different in case combined vertex and index buffers are in use.
    size_t cpuDrawStart;
};

/// Draw call source data with optimal memory storage. 
class SourceBatches
{
public:
    /// Construct.
    SourceBatches();
    /// Destruct.
    ~SourceBatches();

    /// Set number of geometries. Will clear previously set geometry and material assignments.
    void SetNumGeometries(size_t num);

    /// Set geometry at index. %Geometry pointers are raw pointers for safe LOD level changes on OnPrepareRender() in worker threads; a strong ref to the geometry should be held elsewhere.
    void SetGeometry(size_t index, Geometry* geometry)
    {
        if (numGeometries < 2)
            geomPtr = geometry;
        else
            *(reinterpret_cast<Geometry**>(geomPtr) + index * 2) = geometry;
    } 

    /// Set material at index. %Materials hold strong refs and should not be changed from worker threads in OnPrepareRender().
    void SetMaterial(size_t index, Material* material)
    {
        if (numGeometries < 2)
            matPtr = material;
        else
            *(reinterpret_cast<SharedPtr<Material>*>(geomPtr) + index * 2 + 1) = material;
    }

    /// Return number of geometries.
    size_t NumGeometries() const { return numGeometries; }

    /// Return geometry at index.
    Geometry* GetGeometry(size_t index) const
    {
        if (numGeometries < 2)
            return reinterpret_cast<Geometry*>(geomPtr);
        else
            return *(reinterpret_cast<Geometry**>(geomPtr) + index * 2);
    }

    /// Return material at index.
    Material* GetMaterial(size_t index) const
    {
        if (numGeometries < 2)
            return matPtr;
        else
            return *(reinterpret_cast<SharedPtr<Material>*>(geomPtr) + index * 2 + 1);
    }

private:
    /// Geometry pointer or dynamic storage.
    void* geomPtr;
    /// Material pointer.
    SharedPtr<Material> matPtr;
    /// Number of geometries.
    size_t numGeometries;
};

/// Base class for drawables that contain geometry to be rendered.
class GeometryDrawable : public Drawable
{
    friend class GeometryNode;

public:
    /// Construct.
    GeometryDrawable();

    /// Prepare object for rendering. Reset framenumber and calculate distance from camera. Called by Renderer in worker threads. Return false if should not render.
    bool OnPrepareRender(unsigned short frameNumber, Camera* camera) override;
    /// Update GPU resources and set uniforms for rendering. Called by Renderer when geometry type is not static.
    virtual void OnRender(ShaderProgram* program, size_t geomIndex);

    /// Return geometry type.
    GeometryType GetGeometryType() const { return (GeometryType)(Flags() & DF_GEOMETRY_TYPE_BITS); }
    /// Return the draw call source data for direct access.
    const SourceBatches& Batches() const { return batches; }

protected:
    /// Draw call source data.
    SourceBatches batches;
};

/// Base class for scene nodes that contain geometry to be rendered.
class GeometryNode : public OctreeNode
{
    OBJECT(GeometryNode);

public:
    /// Register factory and attributes.
    static void RegisterObject();

    /// Set number of geometries.
    void SetNumGeometries(size_t num);
    /// Set geometry at index.
    void SetGeometry(size_t index, Geometry* geometry);
    /// Set material at every geometry index. Specifying null will use the default material (opaque white.)
    void SetMaterial(Material* material);
    /// Set material at geometry index.
    void SetMaterial(size_t index, Material* material);

    /// Return geometry type.
    GeometryType GetGeometryType() const { return (GeometryType)(drawable->Flags() & DF_GEOMETRY_TYPE_BITS); }
    /// Return number of geometries / batches.
    size_t NumGeometries() const { return static_cast<GeometryDrawable*>(drawable)->batches.NumGeometries(); }
    /// Return geometry by index.
    Geometry* GetGeometry(size_t index) const { return static_cast<GeometryDrawable*>(drawable)->batches.GetGeometry(index); }
    /// Return material by geometry index.
    Material* GetMaterial(size_t index) const { return static_cast<GeometryDrawable*>(drawable)->batches.GetMaterial(index); }
    /// Return the draw call source data for direct access.
    const SourceBatches& Batches() const { return static_cast<GeometryDrawable*>(drawable)->batches; }

protected:
    /// Set materials list. Used in serialization.
    void SetMaterialsAttr(const ResourceRefList& value);
    /// Return materials list. Used in serialization.
    ResourceRefList MaterialsAttr() const;
};
