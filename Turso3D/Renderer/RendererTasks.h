// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Thread/WorkQueue.h"

class LightDrawable;
class Octant;
class Renderer;

/// %Task for collecting octants.
struct CollectOctantsTask : public Task
{
    /// Construct.
    CollectOctantsTask(Renderer* renderer_) :
        renderer(renderer_)
    {
    }

    /// Perform the work. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override;

    /// Renderer this task belongs to.
    Renderer* renderer;
    /// Starting point octant.
    Octant* startOctant;
    /// Result structure index.
    size_t resultIdx;
};

/// %Task for collecting geometry batches from octants.
struct CollectBatchesTask : public Task
{
    /// Construct.
    CollectBatchesTask(Renderer* renderer_) :
        renderer(renderer_)
    {
    }

    /// Perform the work. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override;

    /// Renderer this task belongs to.
    Renderer* renderer;
    /// %Octant list with plane masks.
    std::vector<std::pair<Octant*, unsigned char > > octants;
};

/// %Task for going through scene lights and splitting out further work.
struct ProcessLightsTask : public Task
{
    /// Construct.
    ProcessLightsTask(Renderer* renderer_) :
        renderer(renderer_)
    {
    }

    /// Perform the work. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override;

    /// Renderer this task belongs to.
    Renderer* renderer;
};

/// %Task for culling lights to a specific Z-slice of the frustum grid.
struct CullLightsTask : public Task
{
    /// Construct.
    CullLightsTask(Renderer* renderer_) :
        renderer(renderer_)
    {
    }

    /// Perform the work. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override;

    /// Renderer this task belongs to.
    Renderer* renderer;
    /// Z-slice.
    size_t z;
};

/// %Task for collecting shadowcasters for a light.
struct CollectShadowCastersTask : public Task
{
    /// Construct.
    CollectShadowCastersTask(Renderer* renderer_) :
        renderer(renderer_)
    {
    }

    void Complete(unsigned threadIndex) override;

    /// Renderer this task belongs to.
    Renderer* renderer;
    /// The light to process.
    LightDrawable* light;
};

/// %Task for going through shadowcasters once geometry is also collected, and splitting out further work.
struct ProcessShadowCastersTask : public Task
{
    /// Construct.
    ProcessShadowCastersTask(Renderer* renderer_) :
        renderer(renderer_)
    {
    }

    /// Perform the work. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override;

    /// Renderer this task belongs to.
    Renderer* renderer;
};

/// %Task for collecting shadowcasting batches.
struct CollectShadowBatchesTask : public Task
{
    /// Construct.
    CollectShadowBatchesTask(Renderer* renderer_) :
        renderer(renderer_)
    {
    }

    /// Perform the work. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override;

    /// Renderer this task belongs to.
    Renderer* renderer;
    /// Shadow map index.
    size_t shadowMapIdx;
    /// Shadow view index within shadow map.
    size_t viewIdx;
};

/// %Task for merging and sorting opaque batches of a Z-slice.
struct SortOpaqueBatchesTask : public Task
{
    /// Construct.
    SortOpaqueBatchesTask(Renderer* renderer_) :
        renderer(renderer_)
    {
    }

    /// Perform the work. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override;

    /// Renderer this task belongs to.
    Renderer* renderer;
    /// Z-slice.
    size_t z;
};
