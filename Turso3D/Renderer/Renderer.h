// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "../Object/AutoPtr.h"
#include "../Math/Color.h"
#include "../Math/Frustum.h"
#include "../Resource/Image.h"
#include "Batch.h"

class Camera;
class GeometryNode;
class Graphics;
class Material;
class Octree;
class RenderBuffer;
class Scene;
class VertexBuffer;

/// High-level rendering subsystem. Performs rendering of 3D scenes.
class Renderer : public Object
{
    OBJECT(Renderer);

public:
    /// Construct. Register subsystem and objects. A valid OpenGL context must exist.
    Renderer();
    /// Destruct.
    ~Renderer();

    /// Set size and format of shadow maps. First map is used for a directional light, the second as an atlas for others.
    void SetupShadowMaps(int dirLightSize, int lightAtlasSize, ImageFormat format);
    /// Set global depth bias multipiers for shadow maps.
    void SetShadowDepthBiasMul(float depthBiasMul, float slopeScaleBiasMul);
    /// Prepare view for rendering.
    void PrepareView(Scene* scene, Camera* camera, bool drawShadows);
    /// Render shadowmaps before rendering the view. Last shadow framebuffer will be left bound.
    void RenderShadowMaps();
    /// Render opaque objects into currently set framebuffer and viewport. Additive batches can be optionally rendered into a different framebuffer.
    void RenderOpaque(FrameBuffer* additiveFbo);
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

    /// Set a shader program and bind. Return pointer on success or null otherwise.
    ShaderProgram* SetProgram(const std::string& shaderName, const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
     /// Set float uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, float value);
    /// Set Vector2 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector2& value);
    /// Set Vector3 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector3& value);
    /// Set Vector4 uniform. Low performance, provided for convenience.
    void SetUniform(ShaderProgram* program, const char* name, const Vector4& value);
    /// Draw a quad with current renderstate.
    void DrawQuad();

private:
    /// Find visible objects within frustum.
    void CollectVisibleNodes();
    /// Check which lights affect which objects.
    void CollectLightInteractions(bool drawShadows);
    /// Collect (unlit) shadow batches from geometry nodes and sort them.
    void CollectShadowBatches(ShadowMap& shadowMap, ShadowView& view, const std::vector<GeometryNode*>& potentialShadowCasters, bool checkFrustum, bool checkShadowCaster);
    /// Collect batches from visible objects.
    void CollectNodeBatches();
    /// Sort batches from visible objects.
    void SortNodeBatches();
    /// Render a batch queue.
    void RenderBatches(Camera* camera, const std::vector<Batch>& batches);
    /// Allocate shadow map for light. Return true on success.
    bool AllocateShadowMap(Light* light);
    /// %Octree callback for collecting lights and geometries.
    void CollectGeometriesAndLights(std::vector<OctreeNode*>::const_iterator begin, std::vector<OctreeNode*>::const_iterator end, unsigned char planeMask);
    /// Assign a light list to a node. Creates new light lists as necessary to handle multiple lights.
    void AddLightToNode(GeometryNode* node, Light* light, LightList* lightList);
    /// Define face selection texture for point light shadows.
    void DefineFaceSelectionTextures();
    /// Define vertex data for rendering full-screen quads.
    void DefineQuadVertexBuffer();

    /// Current scene.
    Scene* scene;
    /// Current scene octree.
    Octree* octree;
    /// Current camera.
    Camera* camera;
    /// Camera frustum.
    Frustum frustum;
    /// Geometries in frustum.
    std::vector<GeometryNode*> geometries;
    /// Brightest directional light in frustum.
    Light* dirLight;
    /// Point and spot lights in frustum.
    std::vector<Light*> lights;
    /// Intermediate lit geometries list for processing.
    std::vector<GeometryNode*> litGeometries;
    /// Intermediate filtered shadowcaster list for processing.
    std::vector<GeometryNode*> shadowCasters;
    /// Light lists.
    std::map<unsigned long long, LightList*> lightLists;
    /// Light list allocation pool.
    std::vector<AutoPtr<LightList> > lightListPool;
    /// Shadow maps.
    std::vector<ShadowMap> shadowMaps;
    /// Used light lists so far.
    size_t usedLightLists;
    /// Face selection UV indirection texture 1.
    AutoPtr<Texture> faceSelectionTexture1;
    /// Face selection UV indirection texture 2.
    AutoPtr<Texture> faceSelectionTexture2;
    /// Opaque batches.
    BatchQueue opaqueBatches;
    /// Opaque additive batches.
    BatchQueue opaqueAdditiveBatches;
    /// Transparent batches.
    BatchQueue alphaBatches;
    /// Instancing world transforms.
    std::vector<Matrix3x4> instanceTransforms;
    /// Instancing vertex buffer.
    AutoPtr<VertexBuffer> instanceVertexBuffer;
    /// Quad vertex buffer.
    AutoPtr<VertexBuffer> quadVertexBuffer;
    /// Cached static object shadow buffer.
    AutoPtr<RenderBuffer> staticObjectShadowBuffer;
    /// Cached static object shadow framebuffer.
    AutoPtr<FrameBuffer> staticObjectShadowFbo;
    /// Instancing supported flag.
    bool hasInstancing;
    /// Instancing vertex arrays enabled flag.
    bool instancingEnabled;
    /// Instancing buffer need update flag.
    bool instanceTransformsDirty;
    /// Shadow maps globally dirty flag. All cached shadow content should be reset.
    bool shadowMapsDirty;
    /// Vertex elements for the instancing buffer.
    std::vector<VertexElement> instanceVertexElements;
    /// Camera view mask.
    unsigned viewMask;
    /// Framenumber.
    unsigned short frameNumber;
    /// Subview framenumber for state sorting.
    unsigned short sortViewNumber;
    /// Last camera used for rendering.
    Camera* lastCamera;
    /// Last camera uniforms assignment number.
    unsigned lastPerViewUniforms;
    /// Last light pass used for rendering batches.
    LightPass* lastLightPass;
    /// Last lightlist uniforms assignment number.
    unsigned lastPerLightUniforms;
    /// Last material pass used for rendering.
    Pass* lastPass;
    /// Last material used for rendering.
    Material* lastMaterial;
    /// Last material uniforms assignment number.
    unsigned lastPerMaterialUniforms;
    /// Last cull mode.
    CullMode lastCullMode;
    /// Last blend mode.
    BlendMode lastBlendMode;
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
};

/// Register Renderer related object factories and attributes.
void RegisterRendererLibrary();

