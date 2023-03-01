// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Object/AutoPtr.h"
#include "../Resource/Image.h"
#include "../Thread/WorkQueue.h"
#include "Batch.h"

class Camera;
class GeometryNode;
class Graphics;
class Material;
class Octree;
class RenderBuffer;
class Scene;
class UniformBuffer;
class VertexBuffer;
struct Octant;

static const size_t NUM_CLUSTER_X = 16;
static const size_t NUM_CLUSTER_Y = 8;
static const size_t NUM_CLUSTER_Z = 8;
static const size_t MAX_LIGHTS = 255;
static const size_t MAX_LIGHTS_CLUSTER = 16;

/// Per-thread results from scene geometry processing.
struct ThreadGeometryResult
{
    /// Clear for the next frame.
    void Clear();

    /// Minimum geometry Z value.
    float minZ;
    /// Maximum geometry Z value.
    float maxZ;
    /// Initial opaque batches.
    std::vector<Batch> opaqueBatches;
    /// Initial alpha batches.
    std::vector<Batch> alphaBatches;
};

/// Per-view uniform buffer data
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
    /// Draw a quad with current renderstate.
    void DrawQuad();

private:
    /// Collect batches for visible geometries within frustum, as well as lights. Initiate light grid culling task. Perform octree queries for shadowcasters per light.
    void CollectGeometriesAndLights();
    /// Join the per-thread batch results and sort. Collect and sort shadowcaster geometry batches.
    void JoinAndSortBatches();
    /// Upload instance transforms before rendering.
    void UpdateInstanceTransforms(const std::vector<Matrix3x4>& transforms);
    /// Render a batch queue.
    void RenderBatches(Camera* camera, const BatchQueue& queue);
    /// Allocate shadow map for light. Return true on success.
    bool AllocateShadowMap(Light* light);
    /// Define face selection texture for point light shadows.
    void DefineFaceSelectionTextures();
    /// Define vertex data for rendering full-screen quads.
    void DefineQuadVertexBuffer();
    /// Setup light cluster frustums and bounding boxes if necessary.
    void DefineClusterFrustums();
    /// Work function to collect main view batches from geometries.
    void CollectBatchesWork(Task* task, unsigned threadIndex);
    /// Work function to collect shadowcasters per shadowcasting light.
    void CollectShadowCastersWork(Task* task, unsigned threadIndex);
    /// Work function to collect and sort shadowcaster batches per shadowmap.
    void CollectShadowBatchesWork(Task* task, unsigned threadIndex);
    /// Work function to cull lights to the frustum grid.
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
    /// Octants with plane masks in frustum.
    std::vector<std::pair<Octant*, unsigned char> > octants;
    /// Brightest directional light in frustum.
    Light* dirLight;
    /// Accepted point and spot lights in frustum.
    std::vector<Light*> lights;
    /// Shadow maps.
    std::vector<ShadowMap> shadowMaps;
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
    /// Cluster frustums for lights.
    Frustum clusterFrustums[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Cluster bounding boxes.
    BoundingBox clusterBoundingBoxes[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Amount of lights per cluster.
    unsigned char numClusterLights[NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Cluster data CPU copy.
    unsigned char clusterData[MAX_LIGHTS_CLUSTER * NUM_CLUSTER_X * NUM_CLUSTER_Y * NUM_CLUSTER_Z];
    /// Per-view uniform data CPU copy.
    PerViewUniforms perViewData;
    /// Light constantbuffer data CPU copy.
    LightData lightData[MAX_LIGHTS + 1];
    /// Last projection matrix used to initialize cluster frustums.
    Matrix4 lastClusterFrustumProj;
    /// Per-worker thread scene geometry collection results.
    std::vector<ThreadGeometryResult> geometryResults;
    /// Minimum Z value for all geometries in frustum.
    float minZ;
    /// Maximum Z value for all geometries in frustum.
    float maxZ;
    /// Opaque batches.
    BatchQueue opaqueBatches;
    /// Transparent batches.
    BatchQueue alphaBatches;
    /// Instancing vertex buffer.
    AutoPtr<VertexBuffer> instanceVertexBuffer;
    /// Quad vertex buffer.
    AutoPtr<VertexBuffer> quadVertexBuffer;
    /// Cached static object shadow buffer.
    AutoPtr<RenderBuffer> staticObjectShadowBuffer;
    /// Cached static object shadow framebuffer.
    AutoPtr<FrameBuffer> staticObjectShadowFbo;
    /// Vertex elements for the instancing buffer.
    std::vector<VertexElement> instanceVertexElements;
    /// Instance transforms for opaque and alpha batches.
    std::vector<Matrix3x4> instanceTransforms;
    /// Shadow use flag.
    bool drawShadows;
    /// Instancing supported flag.
    bool hasInstancing;
    /// Instancing vertex arrays enabled flag.
    bool instancingEnabled;
    /// Shadow maps globally dirty flag. All cached shadow content should be reset.
    bool shadowMapsDirty;
    /// Cluster frustums dirty flag.
    bool clusterFrustumsDirty;
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
    /// Tasks for main view batches collection.
    std::vector<AutoPtr<Task> > collectBatchesTasks;
    /// Tasks for shadow light processing.
    std::vector<AutoPtr<Task> > collectShadowCastersTasks;
    /// Tasks for shadow batch collecting and sorting per shadowmap.
    AutoPtr<Task> collectShadowBatchesTasks[2];
    /// Task for light grid culling.
    AutoPtr<Task> cullLightsTask;
};

/// Register Renderer related object factories and attributes.
void RegisterRendererLibrary();

