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

/// Sorting modes for batches.
enum BatchSortMode
{
    SORT_STATE = 0,
    SORT_STATE_AND_DISTANCE,
    SORT_DISTANCE
};

/// Stored draw call.
struct Batch
{
    union
    {
        /// State sorting key.
        unsigned sortKey;
        /// Distance for alpha batches.
        float distance;
        /// Start position in the instance vertex buffer if instanced.
        unsigned instanceStart;
    };

    /// %Material pass.
    Pass* pass;
    /// %Geometry.
    Geometry* geometry;
    /// %Shader variation bits.
    unsigned char programBits;

    union
    {
        /// Owner object, for complex rendering like skinning.
        GeometryNode* node;
        /// Pointer to world transform matrix for static geometry rendering.
        const Matrix3x4* worldTransform;
        /// Instance count if instanced.
        unsigned instanceCount;
    };
};

/// Collection of draw calls with sorting and instancing functionality.
struct BatchQueue
{
    /// Clear.
    void Clear();
    /// Sort batches and setup instancing groups.
    void Sort(BatchSortMode sortMode, bool convertToInstanced);
    /// Return whether has batches added.
    bool HasBatches() const { return batches.size(); }

    /// Batches.
    std::vector<Batch> batches;
    /// Instance transforms
    std::vector<Matrix3x4> instanceTransforms;
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
    /// Shadow map framebuffer.
    SharedPtr<FrameBuffer> fbo;
    /// Shadow views that use this shadow map.
    std::vector<ShadowView*> shadowViews;
    /// Shadow batch queues used by the shadow views.
    std::vector<BatchQueue> shadowBatches;
    /// Initial shadowcaster list from octree query.
    std::vector<GeometryNode*> initialShadowCasters;
    /// Intermediate filtered shadowcaster list for processing.
    std::vector<GeometryNode*> shadowCasters;
    /// Next free batch queue.
    size_t freeQueueIdx;
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
