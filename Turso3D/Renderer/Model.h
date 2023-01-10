// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Graphics/GraphicsDefs.h"
#include "../Math/BoundingBox.h"
#include "../Resource/Resource.h"

class VertexBuffer;
class IndexBuffer;
struct Geometry;

/// Load-time description of a vertex buffer, to be uploaded on the GPU later.
struct VertexBufferDesc
{
    /// Vertex declaration.
    std::vector<VertexElement> vertexElements;
    /// Number of vertices.
    size_t numVertices;
    /// Vertex data.
    SharedArrayPtr<unsigned char> vertexData;
};

/// Load-time description of an index buffer, to be uploaded on the GPU later.
struct IndexBufferDesc
{
    /// Index size.
    size_t indexSize;
    /// Number of indices.
    size_t numIndices;
    /// Index data.
    SharedArrayPtr<unsigned char> indexData;
};

/// Load-time description of a geometry.
struct GeometryDesc
{
    /// LOD distance.
    float lodDistance;
    /// Vertex buffer ref.
    unsigned vbRef;
    /// Index buffer ref.
    unsigned ibRef;
    /// Draw range start.
    unsigned drawStart;
    /// Draw range element count.
    unsigned drawCount;
};

/// Model's bone description.
struct Bone
{
    /// Default-construct.
    Bone();
    /// Destruct.
    ~Bone();

    /// Name.
    std::string name;
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

/// Combined vertex and index buffers for static models.
class CombinedBuffer : public RefCounted
{
public:
    /// Construct with the specified vertex elements.
    CombinedBuffer(const std::vector<VertexElement>& elements);

    /// Update vertex data at current position and advance use counter. Return true if data fit the buffer.
    bool FillVertices(size_t numVertices, const void* data);
    /// Update index data at current position and advance use counter. Return true if data fit the buffer. Note that index data should be 32-bit.
    bool FillIndices(size_t numIndices, const void* data);

    /// Return vertex use count so far.
    size_t UsedVertices() const { return usedVertices; }
    /// Return index use count so far.
    size_t UsedIndices() const { return usedIndices; }
    /// Return the large vertex buffer.
    VertexBuffer* GetVertexBuffer() const { return vertexBuffer; }
    /// Return the large index buffer.
    IndexBuffer* GetIndexBuffer() const { return indexBuffer; }

    /// Allocate space from a buffer and return it for use. New buffers will be created as necessary.
    static CombinedBuffer* Allocate(const std::vector<VertexElement>& vertexElements, size_t numVertices, size_t numIndices);

private:
    /// Large vertex buffer.
    SharedPtr<VertexBuffer> vertexBuffer;
    /// Large index buffer.
    SharedPtr<IndexBuffer> indexBuffer;
    /// Vertex buffer use count so far.
    size_t usedVertices;
    /// Index buffer use count so far.
    size_t usedIndices;

    /// Current buffers.
    static std::map<unsigned, std::vector<WeakPtr<CombinedBuffer> > > buffers;
};

/// 3D model resource.
class Model : public Resource
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
    void SetBones(const std::vector<Bone>& bones, size_t rootBoneIndex);
    /// Set per-geometry bone mappings.
    void SetBoneMappings(const std::vector<std::vector<size_t> >& boneMappings);
    
    /// Return number of geometries.
    size_t NumGeometries() const { return geometries.size(); }
    /// Return number of LOD levels in a geometry.
    size_t NumLodLevels(size_t index) const;
    /// Return the geometry at batch index and LOD level.
    Geometry* GetGeometry(size_t index, size_t lodLevel) const;
    /// Return the LOD geometries at batch index.
    const std::vector<SharedPtr<Geometry> >& LodGeometries(size_t index) const { return geometries[index]; }
    /// Return the local space bounding box.
    const BoundingBox& LocalBoundingBox() const { return boundingBox; }
    /// Return the model's bones.
    const std::vector<Bone>& Bones() const { return bones; }
    /// Return the root bone index.
    size_t RootBoneIndex() const { return rootBoneIndex; }
    /// Return per-geometry bone mapping.
    const std::vector<std::vector<size_t> > BoneMappings() const { return boneMappings; }

private:
    /// Geometry LOD levels.
    std::vector<std::vector<SharedPtr<Geometry> > > geometries;
    /// Local space bounding box.
    BoundingBox boundingBox;
    /// %Model's bones.
    std::vector<Bone> bones;
    /// Root bone index.
    size_t rootBoneIndex;
    /// Per-geometry bone mappings.
    std::vector<std::vector<size_t> > boneMappings;
    /// Combined buffer if in use.
    SharedPtr<CombinedBuffer> combinedBuffer;
    /// Vertex buffer data for loading.
    std::vector<VertexBufferDesc> vbDescs;
    /// Index buffer data for loading.
    std::vector<IndexBufferDesc> ibDescs;
    /// Geometry descriptions for loading.
    std::vector<std::vector<GeometryDesc> > geomDescs;
};
