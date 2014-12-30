// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/AutoPtr.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Math/BoundingBox.h"
#include "../Resource/Resource.h"

namespace Turso3D
{

class VertexBuffer;
class IndexBuffer;
struct Geometry;

/// Load-time description of a vertex buffer, to be uploaded on the GPU later.
struct TURSO3D_API VertexBufferDesc
{
    /// Vertex declaration.
    Vector<VertexElement> vertexElements;
    /// Number of vertices.
    size_t numVertices;
    /// Vertex data.
    AutoArrayPtr<unsigned char> vertexData;
};

/// Load-time description of an index buffer, to be uploaded on the GPU later.
struct TURSO3D_API IndexBufferDesc
{
    /// Index size.
    size_t indexSize;
    /// Number of indices.
    size_t numIndices;
    /// Index data.
    AutoArrayPtr<unsigned char> indexData;
};

/// Model's bone description.
struct TURSO3D_API Bone
{
    /// Default-construct.
    Bone();
    /// Destruct.
    ~Bone();

    /// Name.
    String name;
    /// Reset position.
    Vector3 initialPosition;
    /// Reset rotation.
    Quaternion initialRotation;
    /// Reset scale.
    Vector3 initialScale;
    /// Offset matrix for skinning.
    Matrix3x4 offsetMatrix;
    /// Collision radius.
    float radius;
    /// Collision bounding box.
    BoundingBox boundingBox;
    /// Parent bone index.
    size_t parentIndex;
    /// Associated scene node.
    WeakPtr<Node> node;
    /// Animated flag.
    bool animated;
};

/// 3D model resource.
class TURSO3D_API Model : public Resource
{
    OBJECT(Model);

public:
    /// Construct.
    Model();
    /// Destruct.
    ~Model();

    /// Register object factory.
    static void RegisterObject();

    /// Load model from a stream. Return true on success.
    bool BeginLoad(Stream& source) override;
    /// Finalize model loading in the main thread. Return true on success.
    bool EndLoad() override;

    /// Set number of geometries.
    void SetNumGeometries(size_t num);
    /// Set number of LOD levels in a geometry.
    void SetNumLodLevels(size_t index, size_t num);
    /// Set local space bounding box.
    void SetLocalBoundingBox(const BoundingBox& box);
    /// Set bones.
    void SetBones(const Vector<Bone>& bones, size_t rootBoneIndex);
    /// Set per-geometry bone mappings.
    void SetBoneMappings(const Vector<Vector<size_t> >& boneMappings);
    
    /// Return number of geometries.
    size_t NumGeometries() const { return geometries.Size(); }
    /// Return number of LOD levels in a geometry.
    size_t NumLodLevels(size_t index) const;
    /// Return the geometry at batch index and LOD level.
    Geometry* GetGeometry(size_t index, size_t lodLevel) const;
    /// Return the LOD geometries at batch index.
    const Vector<SharedPtr<Geometry> >& LodGeometries(size_t index) const { return geometries[index]; }
    /// Return the local space bounding box.
    const BoundingBox& LocalBoundingBox() const { return boundingBox; }
    /// Return the model's bones.
    const Vector<Bone>& Bones() const { return bones; }
    /// Return the root bone index.
    size_t RootBoneIndex() const { return rootBoneIndex; }
    /// Return per-geometry bone mapping.
    const Vector<Vector<size_t> > BoneMappings() const { return boneMappings; }

private:
    /// Geometry LOD levels.
    Vector<Vector<SharedPtr<Geometry> > > geometries;
    /// Local space bounding box.
    BoundingBox boundingBox;
    /// %Model's bones.
    Vector<Bone> bones;
    /// Root bone index.
    size_t rootBoneIndex;
    /// Per-geometry bone mappings.
    Vector<Vector<size_t> > boneMappings;
    /// Vertex buffer data for loading.
    AutoPtr<VertexBufferDesc> vbDesc;
    /// Index buffer data for loading.
    AutoPtr<IndexBufferDesc> ibDesc;
};

}
