// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Math.h"
#include "../Object/Object.h"
#include "Timer.h"

#include <vector>

#define USE_PROFILER

/// Profiling data for one block in the profiling tree.
class ProfilerBlock
{
public:
    /// Construct.
    ProfilerBlock(ProfilerBlock* parent, const char* name);
    /// Destruct.
    ~ProfilerBlock();

    /// Start time measurement and increment call count.
    void Begin();
    /// End time measurement.
    void End();
    /// Process stats at the end of frame.
    void EndFrame();
    /// Begin an interval lasting several frames.
    void BeginInterval();
    /// Return a child block; create if necessary.
    ProfilerBlock* FindOrCreateChild(const char* name);

    /// Block name.
    const char* name;
    /// Hires timer for time measurement.
    HiresTimer timer;
    /// Parent block.
    ProfilerBlock* parent;
    /// Child blocks.
    std::vector<AutoPtr<ProfilerBlock > > children;
    /// Current frame's accumulated time.
    long long time;
    /// Current frame's longest call.
    long long maxTime;
    /// Current frame's call count.
    int count;
    /// Previous frame's accumulated time.
    long long frameTime;
    /// Previous frame's longest call.
    long long frameMaxTime;
    /// Previous frame's call count.
    int frameCount;
    /// Current interval's accumulated time.
    long long intervalTime;
    /// Current interval's longest call.
    long long intervalMaxTime;
    /// Current interval's call count.
    int intervalCount;
    /// Accumulated time since start.
    long long totalTime;
    /// Longest call since start.
    long long totalMaxTime;
    /// Call count since start.
    long long totalCount;
};

/// Hierarchical performance profiler subsystem.
class Profiler : public Object
{
    OBJECT(Profiler);

public:
    /// Construct.
    Profiler();
    /// Destruct.
    ~Profiler();

    /// Begin a profiling block. The name must be persistent; string literals are recommended.
    void BeginBlock(const char* name);
    /// End the current profiling block.
    void EndBlock();
    /// Begin the next profiling frame.
    void BeginFrame();
    /// End the current profiling frame.
    void EndFrame();
    /// Begin a profiler interval.
    void BeginInterval();

    /// Output results into a string.
    std::string OutputResults(bool showUnused = false, bool showTotal = false, size_t maxDepth = M_MAX_UNSIGNED) const;
    /// Return the current profiling block.
    const ProfilerBlock* CurrentBlock() const { return current; }
    /// Return the root profiling block.
    const ProfilerBlock* RootBlock() const { return root; }

private:
    /// Output results recursively.
    void OutputResults(ProfilerBlock* block, std::string& output, size_t depth, size_t maxDepth, bool showUnused, bool showTotal) const;

    /// Current profiling block.
    ProfilerBlock* current;
    /// Root profiling block.
    AutoPtr<ProfilerBlock> root;
    /// Frames in the current interval.
    size_t intervalFrames;
    /// Total frames since start.
    size_t totalFrames;
};

/// Helper class for automatically beginning and ending a profiling block
class AutoProfileBlock
{
public:
    /// Construct and begin a profiling block. The name must be persistent; string literals are recommended.
    AutoProfileBlock(const char* name)
    {
        profiler = Object::Subsystem<Profiler>();
        if (profiler)
            profiler->BeginBlock(name);
    }

    /// Destruct. End the profiling block.
    ~AutoProfileBlock()
    {
        if (profiler)
            profiler->EndBlock();
    }

private:
    /// Profiler subsystem.
    Profiler* profiler;
};

#ifdef USE_PROFILER
#define PROFILE(name) AutoProfileBlock profile_ ## name (#name)
#else
#define PROFILE(name)
#endif
