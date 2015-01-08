// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/AutoPtr.h"
#include "../Graphics/Texture.h"
#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Resource/Image.h"
#include "Batch.h"

namespace Turso3D
{

class ConstantBuffer;
class GeometryNode;
class Octree;
class Scene;
class VertexBuffer;

/// Shader constant buffers used by high-level rendering.
enum RendererConstantBuffer
{
    CB_FRAME = 0,
    CB_OBJECT,
    CB_MATERIAL,
    CB_LIGHTS
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
    /// Render shadow maps. Should be called after all CollectBatches() calls but before RenderBatches(). Note that you must reassign your rendertarget and viewport after calling this.
    void RenderShadowMaps();
    /// Render several passes to the currently set rendertarget and viewport. Avoids setting the per-frame constants multiple times.
    void RenderBatches(const Vector<PassDesc>& passes);
    /// Render a pass to the currently set rendertarget and viewport. Convenience function for one pass only.
    void RenderBatches(const String& pass);

private:
    /// Initialize. Needs the Graphics subsystem and rendering context to exist.
    void Initialize();
    /// (Re)define face selection textures.
    void DefineFaceSelectionTextures();
    /// Octree callback for collecting lights and geometries.
    void CollectGeometriesAndLights(Vector<OctreeNode*>::ConstIterator begin, Vector<OctreeNode*>::ConstIterator end, bool inside);
    /// Assign a light list to a node. Creates new light lists as necessary to handle multiple lights.
    void AddLightToNode(GeometryNode* node, Light* light, LightList* lightList);
    /// Collect shadow caster batches.
    void CollectShadowBatches(const Vector<GeometryNode*>& nodes, BatchQueue& batchQueue, const Frustum& frustum, bool checkShadowCaster, bool checkFrustum);
    /// Render batches from a specific queue and camera.
    void RenderBatches(const Vector<Batch>& batches, Camera* camera, bool setPerFrameContants = true, bool overrideDepthBias = false, int depthBias = 0, float slopeScaledDepthBias = 0.0f);
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
    /// Shadow views.
    Vector<AutoPtr<ShadowView> > shadowViews;
    /// Used shadow views so far.
    size_t usedShadowViews;
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

