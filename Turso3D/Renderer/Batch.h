// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/AreaAllocator.h"
#include "../Math/Matrix3x4.h"
#include "../Object/Ptr.h"

#include <vector>

class GeometryDrawable;
class Pass;
struct Geometry;

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
    /// Geometry index.
    unsigned char geomIndex;

    union
    {
        /// Associated drawable. Called into for complex rendering like skinning.
        GeometryDrawable* drawable;
        /// Pointer to world transform matrix for static geometry rendering.
        const Matrix3x4* worldTransform;
        /// Instance count if instanced.
        unsigned instanceCount;
    };
};

/// Collection of draw calls with sorting and instancing functionality.
struct BatchQueue
{
    /// Clear for the next frame.
    void Clear();
    /// Sort batches and setup instancing groups.
    void Sort(std::vector<Matrix3x4>& instanceTransforms, BatchSortMode sortMode, bool convertToInstanced);
    /// Return whether has batches added.
    bool HasBatches() const { return batches.size(); }

    /// Batches.
    std::vector<Batch> batches;
};
