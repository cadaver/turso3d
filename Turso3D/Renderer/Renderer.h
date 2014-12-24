// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/AutoPtr.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Math/Frustum.h"
#include "../Object/Object.h"

namespace Turso3D
{

class Camera;
class ConstantBuffer;
class Light;
class Octree;
class Scene;
class VertexBuffer;
struct SourceBatch;

typedef Vector<Light*> LightQueue;

/// Shader constant buffers used by high-level rendering.
enum RendererConstantBuffer
{
    CB_FRAME = 0,
    CB_OBJECT,
    CB_MATERIAL
};

/// Batch sorting modes.
enum BatchSortMode
{
    SORT_STATE = 0,
    SORT_BACKTOFRONT
};

/// Parameter indices in constant buffers used by high-level rendering.
static const size_t VS_SCENE_VIEW_MATRIX = 0;
static const size_t VS_SCENE_PROJECTION_MATRIX = 1;
static const size_t VS_SCENE_VIEWPROJ_MATRIX = 2;
static const size_t VS_OBJECT_WORLD_MATRIX = 0;

static const unsigned char INSTANCE_TEXCOORD = 4;

/// Description of a draw call.
struct TURSO3D_API Batch
{
    /// Calculate sort key from state.
    void CalculateSortKey();

    /// Geometry.
    Geometry* geometry;
    /// Material pass.
    Pass* pass;
    /// Pointer to light queue.
    LightQueue* lights;
    /// Shader variations.
    ShaderVariation* shaders[MAX_SHADER_STAGES];
    /// Scene node world matrix.
    const Matrix3x4* worldMatrix;

    union
    {
        /// Sort key for state sorting.
        unsigned long long sortKey;
        /// Distance for sorting.
        float distance;
    };
};

/// Description of an instanced draw call.
struct TURSO3D_API InstanceBatch : public Batch
{
    /// Instance world matrices.
    Vector<const Matrix3x4*> worldMatrices;
    /// Start position in the instancing buffer.
    size_t startIndex;
};

/// Instance batch grouping key.
struct TURSO3D_API InstanceKey
{
    /// Construct undefined.
    InstanceKey()
    {
    }

    /// Construct with values.
    InstanceKey(Geometry* geometry_, Pass* pass_, LightQueue* lights_) :
        geometry(geometry_),
        pass(pass_),
        lights(lights_)
    {
    }

    /// Test for equality with another key.
    bool operator == (const InstanceKey& rhs) const { return geometry == rhs.geometry && pass == rhs.pass && lights == rhs.lights; }
    /// Test for inequality with another key.
    bool operator != (const InstanceKey& rhs) const { return !(*this == rhs); }
    /// Convert to hash value
    unsigned ToHash() const { return (unsigned)geometry + (unsigned)pass + (unsigned)lights; }

    /// Geometry.
    Geometry* geometry;
    /// Material pass.
    Pass* pass;
    /// Pointer to light queue.
    LightQueue* lights;
};

/// Collected batches for a pass.
struct TURSO3D_API PassQueue
{
    /// Clear all batches.
    void Clear();
    /// Sort batches.
    void Sort(BatchSortMode sort);

    /// Instanced batches.
    HashMap<InstanceKey, InstanceBatch> instanceBatches;
    /// Sorted instance batches.
    Vector<InstanceBatch*> sortedInstanceBatches;
    /// Non-instanced batches.
    Vector<Batch> batches;
};

/// High-level rendering subsystem. Performs rendering of 3D scenes.
class TURSO3D_API Renderer : public Object
{
    OBJECT(Renderer);

public:
    /// Construct and register subsystem. 
    Renderer();
    /// Destruct.
    ~Renderer();

    /// Initialize rendering of a new view and collect visible objects from the camera's point of view.
    void CollectObjects(Scene* scene, Camera* camera);
    /// Collect and sort batches from a pass.
    void CollectBatches(const String& pass, BatchSortMode sort = SORT_STATE);
    /// Render the collected batches.
    void RenderBatches(const String& pass);

private:
    /// Collect batches and setup sorting by state.
    void CollectBatchesByState(size_t passIndex, PassQueue& passQueue);
    /// Collect batches and setup sorting by distance.
    void CollectBatchesByDistance(size_t passIndex, PassQueue& passQueue);
    /// Set shader variations for a batch after the final geometry type is known.
    void SetBatchShaders(Batch& batch, GeometryType type);
    /// Load shaders for a pass.
    void LoadPassShaders(Pass* pass);
    /// Render a batch.
    void RenderBatch(Batch& batch, bool isInstanced, Pass*& lastPass, Material*& lastMaterial);

    /// Graphics subsystem pointer.
    WeakPtr<Graphics> graphics;
    /// Current scene.
    Scene* scene;
    /// Current camera.
    Camera* camera;
    /// Current octree.
    Octree* octree;
    /// Camera's view frustum.
    Frustum frustum;
    /// Camera's view matrix.
    Matrix3x4 viewMatrix;
    /// Camera's projection matrix.
    Matrix4 projectionMatrix;
    /// Combined view-projection matrix.
    Matrix4 viewProjMatrix;
    /// Found geometry objects.
    Vector<GeometryNode*> geometries;
    /// Render queues per pass.
    HashMap<size_t, PassQueue> passQueues;
    /// Per-frame vertex shader constant buffer.
    AutoPtr<ConstantBuffer> vsFrameConstantBuffer;
    /// Per-fame pixel shader constant buffer.
    AutoPtr<ConstantBuffer> psFrameConstantBuffer;
    /// Per-object vertex shader constant buffer.
    AutoPtr<ConstantBuffer> vsObjectConstantBuffer;
    /// Instance transform vertex buffer.
    AutoPtr<VertexBuffer> instanceVertexBuffer;
    /// Vertex elements for the instance vertex buffer.
    Vector<VertexElement> instanceVertexElements;
    /// Instance transforms for uploading to the instance vertex buffer.
    Vector<Matrix3x4> instanceTransforms;
    /// Instance vertex buffer dirty flag.
    bool instanceTransformsDirty;
};

/// Register Renderer related object factories and attributes.
TURSO3D_API void RegisterRendererLibrary();

}

