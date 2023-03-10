// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Graphics/GraphicsDefs.h"
#include "../IO/JSONValue.h"
#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Object/AutoPtr.h"
#include "../Resource/Image.h"
#include "../Thread/WorkQueue.h"
#include "Batch.h"

#include <atomic>

class Camera;
class FrameBuffer;
class GeometryNode;
class Graphics;
class Light;
class Material;
class Octree;
class RenderBuffer;
class Scene;
class ShaderProgram;
class Texture;
class UniformBuffer;
class VertexBuffer;
struct Octant;
struct ShadowView;

static const size_t NUM_CLUSTER_X = 16;
static const size_t NUM_CLUSTER_Y = 8;
static const size_t NUM_CLUSTER_Z = 8;
static const size_t MAX_LIGHTS = 255;
static const size_t MAX_LIGHTS_CLUSTER = 16;
static const size_t NUM_OCTANT_TASKS = 9;

/// Per-thread results for octant collection.
struct ThreadOctantResult
{
    /// Clear for the next frame.
    void Clear();

    /// Octant list current position.
    std::list<std::vector<std::pair<Octant*, unsigned char> > >::iterator octantListIt;
    /// Node accumulator. When full, queue the next batch collection task.
    size_t nodeAcc;
    /// Batch collection task index.
    size_t batchTaskIdx;
    /// Intermediate octant lists. Stored inside a std::list so that batch collection tasks can begin early without iterator invalidation.
    std::list<std::vector<std::pair<Octant*, unsigned char> > > octants;
    /// Intermediate light list.
    std::vector<Light*> lights;
    /// Tasks for main view batches collection, queued by the octant collection task when it finishes.
    std::vector<AutoPtr<Task> > collectBatchesTasks;
};

/// Per-thread results for batch collection.
struct ThreadBatchResult
{
    /// Clear for the next frame.
    void Clear();

    /// Minimum geometry Z value.
    float minZ;
    /// Maximum geometry Z value.
    float maxZ;
    /// Combined bounding box of the visible geometries.
    BoundingBox geometryBounds;
    /// Initial opaque batches.
    std::vector<Batch> opaqueBatches;
    /// Initial alpha batches.
    std::vector<Batch> alphaBatches;
};

/// Per-view uniform buffer data.
struct PerViewUniforms
{
    /// Current camera's view matrix
    Matrix3x4 viewMatrix;
    /// Current camera's projection matrix.
    Matrix4 projectionMatrix;
    /// Current camera's combined view and projection matrix.
    Matrix4 viewProjMatrix;
    /// Current camera's depth parameters.
    Vector4 depthParameters;
    /// Data for the view's global directional light.
    Vector4 dirLightData[12];
};

/// Shadow map data structure. May be shared by several lights.
struct ShadowMap
{
    /// Default-construct.
    ShadowMap();
    /// Destruct.
    ~ShadowMap();

    /// Clear for the next frame.
    void Clear();

    /// Next free batch queue.
    size_t freeQueueIdx;
    /// Next free shadowcaster list index.
    size_t freeCasterListIdx;
    /// Rectangle allocator.
    AreaAllocator allocator;
    /// Shadow map texture.
    SharedPtr<Texture> texture;
    /// Shadow map framebuffer.
    SharedPtr<FrameBuffer> fbo;
    /// Shadow views that use this shadow map.
    std::vector<ShadowView*> shadowViews;
    /// Shadow batch queues used by the shadow views.
    std::vector<BatchQueue> shadowBatches;
    /// Intermediate shadowcaster lists for processing.
    std::vector<std::vector<GeometryNode*> > shadowCasters;
    /// Instancing transforms for shadowcasters.
    std::vector<Matrix3x4> instanceTransforms;
};

/// Light data for cluster light shader.
struct LightData
{
    /// %Light position.
    Vector4 position;
    /// %Light direction.
    Vector4 direction;
    /// %Light attenuation parameters.
    Vector4 attenuation;
    /// %Light color.
    Color color;
    /// Shadow parameters.
    Vector4 shadowParameters;
    /// Shadow matrix. For point lights, contains extra parameters.
    Matrix4 shadowMatrix;
};

/// High-level rendering subsystem. Performs rendering of 3D scenes.
class Renderer : public Object
{
    OBJECT(Renderer);

public:
    /// Construct. Register subsystem and objects. Graphics and WorkQueue subsystems must have been initialized.
    Renderer();
    /// Destruct.
    ~Renderer();

    /// Set size and format of shadow maps. First map is used for a directional light, the second as an atlas for others.
    void SetupShadowMaps(int dirLightSize, int lightAtlasSize, ImageFormat format);
    /// Set global depth bias multipiers for shadow maps.
    void SetShadowDepthBiasMul(float depthBiasMul, float slopeScaleBiasMul);
    /// Prepare view for rendering. This will utilize worker threads.
    void PrepareView(Scene* scene, Camera* camera, bool drawShadows);
    /// Render shadowmaps before rendering the view. Last shadow framebuffer will be left bound.
    void RenderShadowMaps();
    /// Render opaque objects into currently set framebuffer and viewport.
    void RenderOpaque();
    /// Render transparent objects into currently set framebuffer and viewport.
    void RenderAlpha();

    /// Clear the current framebuffer.
    void Clear(bool clearColor = true, bool clearDepth = true, const IntRect& clearRect = IntRect::ZERO, const Color& backgroundColor = Color::BLACK);
    /// Set the viewport rectangle.
    void SetViewport(const IntRect& viewRect);
    /// Set basic renderstates.
    void SetRenderState(BlendMode blendMode, CullMode cullMode = CULL_BACK, CompareMode depthTest = CMP_LESS, bool colorWrite = true, bool depthWrite = true);
    /// Set depth bias.
    void SetDepthBias(float constantBias = 0.0f, float slopeScaleBias = 0.0f);

    /// Bind a shader program for use. Return pointer on success or null otherwise.
    ShaderProgram* SetProgram(const std::string& shaderName, const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
     /// Set float uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, float value);
    /// Set a Vector2 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector2& value);
    /// Set a Vector3 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector3& value);
    /// Set a Vector4 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector4& value);
    /// Set a Matrix3x4 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Matrix3x4& value);
    /// Set a Matrix4 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Matrix4& value);
    /// Draw a quad with current renderstate.
    void DrawQuad();

    /// Return a shadow map texture by index for debugging.
    Texture* ShadowMapTexture(size_t index) const;

private:
    /// Collect octants and lights from the octree recursively. Queue batch collection tasks while ongoing.
    void CollectOctantsAndLights(Octant* octant, ThreadOctantResult& result, bool threaded, bool recursive, unsigned char planeMask = 0x3f);
    /// Allocate shadow map for light. Return true on success.
    bool AllocateShadowMap(Light* light);
    /// Sort main opaque and alpha batch queues.
    void SortMainBatches();
    /// Sort all batch queues of a shadowmap.
    void SortShadowBatches(ShadowMap& shadowMap);
    /// Upload instance transforms before rendering.
    void UpdateInstanceTransforms(const std::vector<Matrix3x4>& transforms);
    /// Render a batch queue.
    void RenderBatches(Camera* camera, const BatchQueue& queue);
    /// Define face selection texture for point light shadows.
    void DefineFaceSelectionTextures();
    /// Define vertex data for rendering full-screen quads.
    void DefineQuadVertexBuffer();
    /// Setup light cluster frustums and bounding boxes if necessary.
    void DefineClusterFrustums();
    /// Work function to collect octants.
    void CollectOctantsWork(Task* task, unsigned threadIndex);
    /// Process lights collected by octant tasks, and queue shadowcaster query tasks for them as necessary.
    void ProcessLightsWork(Task* task, unsigned threadIndex);
    /// Work function to collect main view batches from geometries.
    void CollectBatchesWork(Task* task, unsigned threadIndex);
    /// Work function to collect shadowcasters per shadowcasting light.
    void CollectShadowCastersWork(Task* task, unsigned threadIndex);
    /// Work function to queue shadowcaster batch collection tasks. Requires batch collection and shadowcaster query tasks to be complete.
    void ProcessShadowCastersWork(Task* task, unsigned threadIndex);
    /// Work function to collect shadowcaster batches per shadow view.
    void CollectShadowBatchesWork(Task* task, unsigned threadIndex);
    /// Work function to cull lights against a Z-slice of the frustum grid.
    void CullLightsToFrustumWork(Task* task, unsigned threadIndex);

    /// Current scene.
    Scene* scene;
    /// Current scene octree.
    Octree* octree;
    /// Current camera.
    Camera* camera;
    /// Camera frustum.
    Frustum frustum;
    /// Cached work queue subsystem.
    WorkQueue* workQueue;
    /// Camera view mask.
    unsigned viewMask;
    /// Framenumber.
    unsigned short frameNumber;
    /// Shadow use flag.
    bool drawShadows;
    /// Shadow maps globally dirty flag. All cached shadow content should be reset.
    bool shadowMapsDirty;
    /// Cluster frustums dirty flag.
    bool clusterFrustumsDirty;
    /// Instancing supported flag.
    bool hasInstancing;
    /// Root-level octants, used as a starting point for octant and batch collection. The root octant is included if it also contains nodes.
    std::vector<Octant*> rootLevelOctants;
    /// Counter for batch collection tasks remaining. When zero, main batch sorting can begin while other tasks go on.
    std::atomic<int> numPendingBatchTasks;
    /// Counters for shadow views remaining per shadowmap. When zero, the shadow batches can be sorted.
    std::atomic<int> numPendingShadowViews[2];
    /// Per-worker thread octant collection results.
    std::vector<ThreadOctantResult> octantResults;
    /// Per-worker thread batch collection results.
    std::vector<ThreadBatchResult> batchResults;
    /// Minimum Z value for all geometries in frustum.
    float minZ;
    /// Maximum Z value for all geometries in frustum.
    float maxZ;
    /// Combined bounding box of the visible geometries.
    BoundingBox geometryBounds;
    /// Brightest directional light in frustum.
    Light* dirLight;
    /// Accepted point and spot lights in frustum.
    std::vector<Light*> lights;
    /// Shadow maps.
    std::vector<ShadowMap> shadowMaps;
    /// Opaque batches.
    BatchQueue opaqueBatches;
    /// Transparent batches.
    BatchQueue alphaBatches;
    /// Instance transforms for opaque and alpha batches.
    std::vector<Matrix3x4> instanceTransforms;
    /// Last camera used for rendering.
    Camera* lastCamera;
    /// Last material pass used for rendering.
    Pass* lastPass;
    /// Last material used for rendering.
    Material* lastMaterial;
    /// Last material uniforms assignment number.
    unsigned lastPerMaterialUniforms;
    /// Last blend mode.
    BlendMode lastBlendMode;
    /// Last cull mode.
    CullMode lastCullMode;
    /// Last depth test.
    CompareMode lastDepthTest;
    /// Instancing vertex arrays enabled flag.
    bool instancingEnabled;
    /// Last color write.
    bool lastColorWrite;
    /// Last depth write.
    bool lastDepthWrite;
    /// Last depth bias enabled.
    bool lastDepthBias;
    /// Constant depth bias multiplier.
    float depthBiasMul;
    /// Slope-scaled depth bias multiplier.
    float slopeScaleBiasMul;
    /// Tasks for octant collection.
    AutoPtr<Task> collectOctantsTasks[NUM_OCTANT_TASKS];
    /// Task for light processing.
    AutoPtr<Task> processLightsTask;
    /// Tasks for shadow light processing.
    std::vector<AutoPtr<Task> > collectShadowCastersTasks;
    /// Task for queuing shadow views for further processing.
    AutoPtr<Task> processShadowCastersTask;
    /// Tasks for shadow batch processing.
    std::vector<AutoPtr<Task> > collectShadowBatchesTasks;
    /// Tasks for light grid culling.
    AutoPtr<Task> cullLightsTasks[NUM_CLUSTER_Z];
    /// Face selection UV indirection texture 1.
    AutoPtr<Texture> faceSelectionTexture1;
    /// Face selection UV indirection texture 2.
    AutoPtr<Texture> faceSelectionTexture2;
    /// Cluster lookup 3D texture.
    AutoPtr<Texture> clusterTexture;
    /// Per-view uniform buffer.
    AutoPtr<UniformBuffer> perViewDataBuffer;
    /// Light data uniform buffer.
    AutoPtr<UniformBuffer> lightDataBuffer;
    /// Instancing vertex buffer.
    AutoPtr<VertexBuffer> instanceVertexBuffer;
    /// Quad vertex buffer.
    AutoPtr<VertexBuffer> quadVertexBuffer;
    /// Cached static object shadow buffer. Note: only needed for the light atlas, not the directional light shadowmap.
    AutoPtr<RenderBuffer> staticObjectShadowBuffer;
    /// Cached static object shadow framebuffer.
    AutoPtr<FrameBuffer> staticObjectShadowFbo;
    /// Vertex elements for the instancing buffer.
    std::vector<VertexElement> instanceVertexElements;
    /// Last projection matrix used to initialize cluster frustums.
    Matrix4 lastClusterFrustumProj;
    /// Amount of lights per cluster.
    unsigned char numClusterLights[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Cluster frustums for lights.
    AutoArrayPtr<Frustum> clusterFrustums;
    /// Cluster bounding boxes.
    AutoArrayPtr<BoundingBox> clusterBoundingBoxes;
    /// Cluster data CPU copy.
    AutoArrayPtr<unsigned char> clusterData;
    /// Light constantbuffer data CPU copy.
    AutoArrayPtr<LightData> lightData;
    /// Per-view uniform data CPU copy.
    PerViewUniforms perViewData;
};

/// Register Renderer related object factories and attributes.
void RegisterRendererLibrary();
