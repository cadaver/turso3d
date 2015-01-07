// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/AreaAllocator.h"
#include "Camera.h"
#include "GeometryNode.h"
#include "Material.h"

namespace Turso3D
{

class Light;
struct LightPass;

/// Maximum number of lights per pass.
static const size_t MAX_LIGHTS_PER_PASS = 4;

/// Batch sorting modes.
enum BatchSortMode
{
    SORT_NONE = 0,
    SORT_STATE,
    SORT_BACK_TO_FRONT,
    SORT_FRONT_TO_BACK,
};

/// Description of a draw call.
struct TURSO3D_API Batch
{
    /// Calculate sort key for state sorting.
    void CalculateSortKey()
    {
        sortKey = ((((unsigned long long)pass->ShaderHash() * type) & 0xffff) << 48) |
            ((((unsigned long long)lights) & 0xffff) << 32) |
            ((((unsigned long long)pass->Parent()) & 0xffff) << 16) |
            (((unsigned long long)geometry) & 0xffff);
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
        /// Instanced mode start index.
        size_t instanceStart;
    };

    union
    {
        /// Sort key for state sorting.
        unsigned long long sortKey;
        /// Distance for sorting.
        float distance;
        /// Instanced mode instance count.
        size_t instanceCount;
    };
};

/// Per-pass batch queue structure.
struct TURSO3D_API BatchQueue
{
    /// Clear structures.
    void Clear();
    /// Sort batches and build instances.
    void Sort(Vector<Matrix3x4>& instanceTransforms);

    /// Build instances from adjacent batches with same state.
    static void BuildInstances(Vector<Batch>& batches, Vector<Matrix3x4>& instanceTransforms);

    /// Batches, which may be instanced or non-instanced.
    Vector<Batch> batches;
    /// Additive lighting batches.
    Vector<Batch> additiveBatches;
    /// Sorting mode.
    BatchSortMode sort;
    /// Lighting flag.
    bool lit;
    /// Base pass index.
    unsigned char baseIndex;
    /// Additive pass index (if needed.)
    unsigned char additiveIndex;
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

/// Shadow rendering view data structure.
struct TURSO3D_API ShadowView
{
    /// Clear existing shadow casters and batch queue.
    void Clear();

    /// %Light that is using this view.
    Light* light;
    /// Viewport within the shadow map.
    IntRect viewport;
    /// Shadow batch queue.
    BatchQueue shadowQueue;
    /// Shadow camera.
    Camera shadowCamera;
};

/// Shadow map data structure. May be shared by several lights.
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
    /// Use flag. When false, clearing the shadow map and rendering the views can be skipped.
    bool used;
};

}