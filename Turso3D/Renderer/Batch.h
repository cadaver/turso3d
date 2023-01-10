// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/AreaAllocator.h"
#include "../Math/Matrix3x4.h"
#include "../Object/Ptr.h"

#include <vector>

class FrameBuffer;
class GeometryNode;
class Light;
class Pass;
class ShaderProgram;
class Texture;
struct Geometry;
struct ShadowView;

static const int MAX_LIGHTS_PER_PASS = 4;

/// Lights affecting a draw call.
struct LightPass
{
    /// Number of lights.
    unsigned char numLights;
    /// Shader program light bits.
    unsigned char lightBits;
    /// Light data. Shadowed lights are stored first.
    Vector4 lightData[MAX_LIGHTS_PER_PASS * 9];
    /// Last sort key for combined distance and state sorting.
    std::pair<unsigned short, unsigned short> lastSortKey;
};

/// List of lights for a geometry node.
struct LightList
{
    /// Lookup key. Used to calculate key for light list based on this one.
    unsigned long long key;
    /// Use count
    size_t useCount;
    /// Lights.
    std::vector<Light*> lights;
    /// Light rendering passes.
    std::vector<LightPass> lightPasses;
};

/// Stored draw call.
struct Batch
{
    // Define state sort key.
    void SetStateSortKey(unsigned short distance);

    /// Light pass, or null if not lit.
    LightPass* lightPass;
    /// Shader program.
    ShaderProgram* program;
    /// Material pass.
    Pass* pass;
    /// Geometry.
    Geometry* geometry;

    union
    {
        /// Owner object, for complex rendering like skinning.
        GeometryNode* node;
        /// Pointer to world transform matrix for static geometry rendering.
        const Matrix3x4* worldTransform;
        /// Instancing start and size.
        unsigned instanceRange[2];
    };

    union
    {
        /// Sort key.
        unsigned long long sortKey;
        /// Distance for alpha batches.
        float distance;
    };
};

/// Collection of draw calls with sorting and instancing functionality.
struct BatchQueue
{
    /// Clear.
    void Clear();
    /// Sort batches and setup instancing groups.
    void Sort(std::vector<Matrix3x4>& instanceTransforms, bool sortByState, bool convertToInstanced);
    /// Return whether has batches added.
    bool HasBatches() const { return batches.size(); }

    /// Unsorted batches.
    std::vector<Batch> batches;
};

/// Shadow map data structure. May be shared by several lights.
struct ShadowMap
{
    /// Default-construct.
    ShadowMap();
    /// Destruct.
    ~ShadowMap();

    /// Clear allocator and batch queue use count.
    void Clear();

    /// Rectangle allocator.
    AreaAllocator allocator;
    /// Shadow map texture.
    SharedPtr<Texture> texture;
    /// Shadow map FBO.
    SharedPtr<FrameBuffer> fbo;
    /// Shadow views that use this shadow map.
    std::vector<ShadowView*> shadowViews;
    /// Shadow batch queues used by the shadow views.
    std::vector<BatchQueue> shadowBatches;
    /// Next free batch queue.
    size_t freeQueueIdx;
};
