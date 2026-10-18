// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/Log.h"
#include "../Thread/WorkQueue.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <chrono>
#include <thread>
#include <tracy/Tracy.hpp>

/// WorkQueue based job system for Jolt.
class JoltJobSystem : public JPH::JobSystemWithBarrier
{
    /// Jolt job executed within the WorkQueue.
    struct WorkQueueJob : public Task, public JPH::JobSystem::Job
    {
        WorkQueueJob(const char* inJobName, JPH::ColorArg inColor, JobSystem* inJobSystem, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies) :
            JPH::JobSystem::Job(inJobName, inColor, inJobSystem, inJobFunction, inNumDependencies)
        {
        }
        
        void Complete(unsigned /*threadIndex*/) override
        {
            ZoneScopedN("UpdatePhysics");
            
            Job* job = static_cast<Job*>(this);
            job->Execute();
            job->Release();
        }
    };

public:
    /// Construct with parameters.
    JoltJobSystem(JPH::uint maxJobs = JPH::cMaxPhysicsJobs, JPH::uint maxBarriers = JPH::cMaxPhysicsBarriers) :
        JPH::JobSystemWithBarrier(maxBarriers),
        workQueue(Object::Subsystem<WorkQueue>())
    {
        jobs.Init(maxJobs, maxJobs);
    }

    /// Destruct.
    ~JoltJobSystem() override
    {
    }

    /// Return max simultaneously executed jobs.
    int GetMaxConcurrency() const override
    {
        return (int)workQueue->NumThreads();
    }

    /// Create a job and queue it immediately for execution if no dependencies.
    JPH::JobHandle CreateJob(const char* name, JPH::ColorArg color, const JobFunction& jobFunction, JPH::uint32 numDependencies = 0) override
    {
        // Obtain a job from the free list, wait for one to become available if the pool is exhausted
        JPH::uint32 index;
        for (;;)
        {
            index = jobs.ConstructObject(name, color, this, jobFunction, numDependencies);
            if (index != AvailableJobs::cInvalidObjectIndex)
                break;
            LOGWARNING("JoltJobSystem: no jobs available, increase maxJobs!");
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        WorkQueueJob* job = &jobs.Get(index);

        // Handle keeps the job alive, the job may start and finish immediately below
        JobHandle handle(job);

        if (numDependencies == 0)
            QueueJob(job);

        return handle;
    }

    /// Queue a job.
    void QueueJob(Job* inJob) override
    {
        // The WorkQueue's Task owns one reference until it has been executed
        inJob->AddRef();
        workQueue->QueueTask(static_cast<WorkQueueJob*>(inJob));
    }

    /// Queue multiple jobs.
    void QueueJobs(Job** inJobs, JPH::uint inNumJobs) override
    {
        static const JPH::uint batchSize = 128;
        Task* batch[batchSize];

        while (inNumJobs > 0)
        {
            JPH::uint count = inNumJobs < batchSize ? inNumJobs : batchSize;
            for (JPH::uint i = 0; i < count; ++i)
            {
                inJobs[i]->AddRef();
                batch[i] = static_cast<WorkQueueJob*>(inJobs[i]);
            }
            workQueue->QueueTasks(count, batch);
            inJobs += count;
            inNumJobs -= count;
        }
    }

    /// Free a job after execution.
    void FreeJob(Job* inJob) override
    {
        jobs.DestructObject(static_cast<WorkQueueJob*>(inJob));
    }

    /// %WorkQueue subsystem.
    WorkQueue* workQueue;
    
    using AvailableJobs = JPH::FixedSizeFreeList<WorkQueueJob>;

    // Job pool.
    AvailableJobs jobs;
};
