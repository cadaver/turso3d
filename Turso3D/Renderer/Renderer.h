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

/// Shader constant buffers used by high-level rendering.
enum RendererConstantBuffer
{
    CB_FRAME = 0,
    CB_OBJECT,
    CB_MATERIAL,
    CB_LIGHTS
};

/// Batch sorting modes.
enum BatchSortMode
{
    SORT_STATE = 0,
    SORT_BACK_TO_FRONT
};

/// Parameter indices in constant buffers used by high-level rendering.
static const size_t VS_FRAME_VIEW_MATRIX = 0;
static const size_t VS_FRAME_PROJECTION_MATRIX = 1;
static const size_t VS_FRAME_VIEWPROJ_MATRIX = 2;
static const size_t PS_FRAME_AMBIENT_COLOR = 0;
static const size_t VS_OBJECT_WORLD_MATRIX = 0;
static const size_t PS_LIGHT_POSITIONS = 0;
static const size_t PS_LIGHT_DIRECTIONS = 1;
static const size_t PS_LIGHT_ATTENUATIONS = 2;
static const size_t PS_LIGHT_COLORS = 3;

/// Texture coordinate index for the instance world matrix.
static const unsigned char INSTANCE_TEXCOORD = 4;

/// Maximum number of lights per pass.
static const size_t MAX_LIGHTS_PER_PASS = 4;

/// Description of lights for a draw call.
struct TURSO3D_API LightQueue
{
    /// Light positions.
    Vector4 lightPositions[MAX_LIGHTS_PER_PASS];
    /// Light directions.
    Vector4 lightDirections[MAX_LIGHTS_PER_PASS];
    /// Light attenuation parameters.
    Vector4 lightAttenuations[MAX_LIGHTS_PER_PASS];
    /// Light colors.
    Color lightColors[MAX_LIGHTS_PER_PASS];
    /// Pixel shader variation index.
    unsigned psIdx;
};

/// Description of a draw call.
struct TURSO3D_API Batch
{
    /// Calculate sort key from state.
    void CalculateSortKey(bool isAdditive = false);

    /// Geometry.
    Geometry* geometry;
    /// Material pass.
    Pass* pass;
    /// Pointer to light queue.
    LightQueue* lights;
    /// Geometry type.
    GeometryType type;

    union
    {
        /// Non-instanced use world matrix.
        const Matrix3x4* worldMatrix;
        /// Index to instance data structures.
        size_t instanceDataIndex;
    };

    union
    {
        /// Sort key for state sorting.
        unsigned long long sortKey;
        /// Distance for sorting.
        float distance;
    };
};

/// Extra data for instanced batch.
struct TURSO3D_API InstanceData
{
    /// World matrices.
    Vector<const Matrix3x4*> worldMatrices;
    /// Start index in the global instance transforms buffer.
    size_t startIndex;
    /// Skip flag. If set, has been converted from non-instanced batches and (size - 1) batches should be skipped after drawing this.
    bool skipBatches;
};

/// Per-pass batch queue structure.
struct TURSO3D_API BatchQueue
{
    /// Clear structures.
    void Clear();
    
    /// Batches, which may be instanced or non-instanced.
    Vector<Batch> batches;
    /// Extra data for instanced batches.
    Vector<InstanceData> instanceDatas;
    /// Indices for instance datas by state sort key. Used only in state-sorted mode.
    HashMap<unsigned long long, size_t> instanceLookup;
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
    /// Collect light interactions with geometries from the current view.
    void CollectLightInteractions();
    /// Collect and sort batches from a pass.
    void CollectBatches(const String& pass, BatchSortMode sort = SORT_STATE, bool lit = true);
    /// Render the collected batches.
    void RenderBatches(const String& pass);

private:
    /// Find a light queue for a geometry node.
    LightQueue* FindLightQueue(const Vector<Light*>& nodeLights, size_t lightStartIndex);
    /// Load shaders for a pass.
    void LoadPassShaders(Pass* pass);
    /// Return or create a shader variation for a pass. Vertex shader variations handle different geometry types and pixel shader variations handle different light combinations.
    ShaderVariation* FindShaderVariation(ShaderStage stage, Pass* pass, size_t idx);
    
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
    /// Per-frame vertex shader constant buffer.
    AutoPtr<ConstantBuffer> vsFrameConstantBuffer;
    /// Per-frame pixel shader constant buffer.
    AutoPtr<ConstantBuffer> psFrameConstantBuffer;
    /// Per-object vertex shader constant buffer.
    AutoPtr<ConstantBuffer> vsObjectConstantBuffer;
    /// Light queue pixel shader constant buffer.
    AutoPtr<ConstantBuffer> psLightConstantBuffer;
    /// Instance transform vertex buffer.
    AutoPtr<VertexBuffer> instanceVertexBuffer;
    /// Vertex elements for the instance vertex buffer.
    Vector<VertexElement> instanceVertexElements;
    /// Geometries in frustum.
    Vector<GeometryNode*> geometries;
    /// Lights in frustum.
    Vector<Light*> lights;
    /// Batch queues per pass.
    HashMap<size_t, BatchQueue> batchQueues;
    /// Instance transforms for uploading to the instance vertex buffer.
    Vector<Matrix3x4> instanceTransforms;
    /// Lit geometries query result.
    Vector<GeometryNode*> litGeometries;
    /// Light queues used by rendering.
    HashMap<unsigned long long, LightQueue> lightQueues;
    /// Current frame number.
    unsigned frameNumber;
    /// Instance vertex buffer dirty flag.
    bool instanceTransformsDirty;
    /// Objects prepared flag.
    bool objectsPrepared;
    /// Per frame constants set flag.
    bool perFrameConstantsSet;
};

/// Register Renderer related object factories and attributes.
TURSO3D_API void RegisterRendererLibrary();

}

