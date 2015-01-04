// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/AutoPtr.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Graphics/Texture.h"
#include "../Math/AreaAllocator.h"
#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Resource/Image.h"
#include "Camera.h"
#include "GeometryNode.h"
#include "Material.h"

namespace Turso3D
{

class ConstantBuffer;
class GeometryNode;
class Light;
class Octree;
class Scene;
class VertexBuffer;
struct BatchQueue;
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
static const size_t VS_FRAME_DEPTH_PARAMETERS = 3;
static const size_t VS_OBJECT_WORLD_MATRIX = 0;
static const size_t VS_LIGHT_SHADOW_MATRICES = 0;
static const size_t PS_FRAME_AMBIENT_COLOR = 0;
static const size_t PS_LIGHT_POSITIONS = 0;
static const size_t PS_LIGHT_DIRECTIONS = 1;
static const size_t PS_LIGHT_ATTENUATIONS = 2;
static const size_t PS_LIGHT_COLORS = 3;
static const size_t PS_LIGHT_SHADOW_PARAMETERS = 4;
static const size_t PS_LIGHT_DIR_SHADOW_SPLITS = 5;
static const size_t PS_LIGHT_DIR_SHADOW_FADE = 6;
static const size_t PS_LIGHT_POINT_SHADOW_PARAMETERS = 7;

/// Texture coordinate index for the instance world matrix.
static const size_t INSTANCE_TEXCOORD = 4;

/// Maximum number of lights per pass.
static const size_t MAX_LIGHTS_PER_PASS = 4;

/// Description of a pass from the client to the renderer.
struct TURSO3D_API PassDesc
{
    /// Construct undefined.
    PassDesc()
    {
    }
    
    /// Construct with parameters.
    PassDesc(const String& name_, BatchSortMode sort_ = SORT_STATE, bool lit_ = true) :
        name(name_),
        sort(sort_),
        lit(lit_)
    {
    }

    /// %Pass name.
    String name;
    /// Sorting mode.
    BatchSortMode sort;
    /// Lighting flag.
    bool lit;
};

/// %Light information for a rendering pass, including properly formatted constant data.
struct TURSO3D_API LightPass
{
    /// %Light positions.
    Vector4 lightPositions[MAX_LIGHTS_PER_PASS];
    /// %Light directions.
    Vector4 lightDirections[MAX_LIGHTS_PER_PASS];
    /// %Light attenuation parameters.
    Vector4 lightAttenuations[MAX_LIGHTS_PER_PASS];
    /// %Light colors.
    Color lightColors[MAX_LIGHTS_PER_PASS];
    /// Shadow map sampling parameters.
    Vector4 shadowParameters[MAX_LIGHTS_PER_PASS];
    /// Point light shadow viewport parameters.
    Vector4 pointShadowParameters[MAX_LIGHTS_PER_PASS];
    /// Directional light shadow split depths.
    Vector4 dirShadowSplits;
    /// Directional light shadow fade parameters.
    Vector4 dirShadowFade;
    /// Shadow mapping matrices.
    Matrix4 shadowMatrices[MAX_LIGHTS_PER_PASS];
    /// Shadow maps.
    Texture* shadowMaps[MAX_LIGHTS_PER_PASS];
    /// Vertex shader variation bits.
    unsigned short vsBits;
    /// Pixel shader variation bits.
    unsigned short psBits;
};

/// %List of lights for a geometry node.
struct TURSO3D_API LightList
{
    /// %List key.
    unsigned long long key;
    /// Lights.
    Vector<Light*> lights;
    /// Associated light passes.
    Vector<LightPass*> lightPasses;
    /// Use count
    size_t useCount;
};

/// Description of a draw call.
struct TURSO3D_API Batch
{
    /// Calculate sort key for state sorting.
    void CalculateSortKey(bool isAdditive)
    {
        sortKey = ((((unsigned long long)pass->Parent() * (unsigned long long)lights * type) & 0x7fffffff) << 32) |
            (((unsigned long long)geometry) & 0xffffffff);
        // Ensure that additive passes are drawn after base passes
        if (isAdditive)
            sortKey |= 0x8000000000000000;
    }

    /// Geometry.
    Geometry* geometry;
    /// Material pass.
    Pass* pass;
    /// Light pass.
    LightPass* lights;
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
    /// Sort batches and build instances in distance sorted mode.
    void Sort();
    /// Add a batch.
    void AddBatch(Batch& batch, GeometryNode* node, bool isAdditive);
    
    /// Batches, which may be instanced or non-instanced.
    Vector<Batch> batches;
    /// Extra data for instanced batches.
    Vector<InstanceData> instanceDatas;
    /// Indices for instance datas by state sort key. Used only in state-sorted mode.
    HashMap<unsigned long long, size_t> instanceLookup;
    /// Sorting mode.
    BatchSortMode sort;
    /// Lighting flag.
    bool lit;
    /// Base pass index.
    unsigned char baseIndex;
    /// Additive pass index (if needed.)
    unsigned char additiveIndex;
};

/// Shadow rendering view.
struct TURSO3D_API ShadowView
{
    /// Clear existing shadow casters and batch queue.
    void Clear();

    /// Light that is using this view.
    Light* light;
    /// Viewport within the shadow map.
    IntRect viewport;
    /// Shadow caster geometries.
    Vector<GeometryNode*> shadowCasters;
    /// Shadow batch queue.
    BatchQueue shadowQueue;
    /// Shadow camera.
    Camera shadowCamera;
};

/// Shadow map data structure.
struct TURSO3D_API ShadowMap
{
    /// Default-construct.
    ShadowMap();
    /// Destruct.
    ~ShadowMap();

    /// Clear allocator and use flag.
    void Clear();

    /// Rectangle allocator.
    AreaAllocator allocator;
    /// Shadow map texture.
    SharedPtr<Texture> texture;
    /// Shadow views that use this shadow map.
    Vector<ShadowView*> shadowViews;
    /// Use flag. When false, clearing and rendering the shadow map can be skipped.
    bool used;
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

    /// Set number, size and format of shadow maps. These will be divided among the lights that need to render shadow maps.
    void SetupShadowMaps(size_t num, int size, ImageFormat format);
    /// Prepare a view for rendering. Convenience function that calls CollectObjects(), CollectLightInteractions() and CollectBatches() in one go. Return true on success.
    bool PrepareView(Scene* scene, Camera* camera, const Vector<PassDesc>& passes);
    /// Initialize rendering of a new view and collect visible objects from the camera's point of view. Return true on success (scene, camera and octree are non-null.)
    bool CollectObjects(Scene* scene, Camera* camera);
    /// Collect light interactions with geometries from the current view. If lights are shadowed, collects batches for shadow casters.
    void CollectLightInteractions();
    /// Collect and sort batches from the visible objects. To not go through the objects several times, all the passes should be specified at once instead of multiple calls to CollectBatches().
    void CollectBatches(const Vector<PassDesc>& passes);
    /// Collect and sort batches from the visible objects. Convenience function for one pass only.
    void CollectBatches(const PassDesc& pass);
    /// Collect and sort batches from the visible objects.
    void CollectBatches(size_t numPasses, const PassDesc* passes);
    /// Render shadow maps. Should be called after all CollectBatches() calls but before RenderBatches(). Note that you must reassign your rendertarget and viewport after calling this.
    void RenderShadowMaps();
    /// Render a specific pass to the currently set rendertarget and viewport.
    void RenderBatches(const String& pass);

private:
    /// Initialize. Needs the Graphics subsystem and rendering context to exist.
    void Initialize();
    /// (Re)define face selection textures.
    void DefineFaceSelectionTextures();
    /// Octree callback for collecting lights and geometries.
    void CollectGeometriesAndLights(Vector<OctreeNode*>::ConstIterator begin, Vector<OctreeNode*>::ConstIterator end, bool inside);
    /// Collect shadow caster batches.
    void CollectShadowBatches(ShadowView* view);
    /// Assign a light list to a node. Creates new light lists as necessary to handle multiple lights.
    void AddLightToNode(GeometryNode* node, Light* light, LightList* lightList);
    /// Sort batch queue. For distance sorted queues, build instances after sorting.
    void SortBatches(BatchQueue& batchQueue, BatchSortMode sort);
    /// Copy instance transforms from batch queue to the global vector.
    void CopyInstanceTransforms(BatchQueue& batchQueue);
    /// Render batches from a specific queue and camera.
    void RenderBatches(BatchQueue& batchQueue, Camera* camera, bool overrideDepthBias = false, int depthBias = 0, float slopeScaledDepthBias = 0.0f);
    /// Load shaders for a pass.
    void LoadPassShaders(Pass* pass);
    /// Return or create a shader variation for a pass. Vertex shader variations handle different geometry types and pixel shader variations handle different light combinations.
    ShaderVariation* FindShaderVariation(ShaderStage stage, Pass* pass, unsigned short bits);
    
    /// Graphics subsystem pointer.
    WeakPtr<Graphics> graphics;
    /// Current scene.
    Scene* scene;
    /// Current scene camera.
    Camera* camera;
    /// Current octree.
    Octree* octree;
    /// Camera's view frustum.
    Frustum frustum;
    /// Camera's view mask.
    unsigned viewMask;
    /// Geometries in frustum.
    Vector<GeometryNode*> geometries;
    /// Lights in frustum.
    Vector<Light*> lights;
    /// Batch queues per pass.
    HashMap<unsigned char, BatchQueue> batchQueues;
    /// Instance transforms for uploading to the instance vertex buffer.
    Vector<Matrix3x4> instanceTransforms;
    /// Lit geometries query result.
    Vector<GeometryNode*> litGeometries;
    /// %Light lists.
    HashMap<unsigned long long, LightList> lightLists;
    /// %Light passes.
    HashMap<unsigned long long, LightPass> lightPasses;
    /// Ambient only light pass.
    LightPass ambientLightPass;
    /// Current frame number.
    unsigned frameNumber;
    /// Instance vertex buffer dirty flag.
    bool instanceTransformsDirty;
    /// Shadow maps.
    Vector<ShadowMap> shadowMaps;
    /// Per-frame vertex shader constant buffer.
    AutoPtr<ConstantBuffer> vsFrameConstantBuffer;
    /// Per-frame pixel shader constant buffer.
    AutoPtr<ConstantBuffer> psFrameConstantBuffer;
    /// Per-object vertex shader constant buffer.
    AutoPtr<ConstantBuffer> vsObjectConstantBuffer;
    /// Lights vertex shader constant buffer.
    AutoPtr<ConstantBuffer> vsLightConstantBuffer;
    /// Lights pixel shader constant buffer.
    AutoPtr<ConstantBuffer> psLightConstantBuffer;
    /// Instance transform vertex buffer.
    AutoPtr<VertexBuffer> instanceVertexBuffer;
    /// Vertex elements for the instance vertex buffer.
    Vector<VertexElement> instanceVertexElements;
    /// First point light face selection cube map.
    AutoPtr<Texture> faceSelectionTexture1;
    /// Second point light face selection cube map.
    AutoPtr<Texture> faceSelectionTexture2;
};

/// Register Renderer related object factories and attributes.
TURSO3D_API void RegisterRendererLibrary();

}

