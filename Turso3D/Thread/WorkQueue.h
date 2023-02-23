// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/Object.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

/// Task for execution by worker threads.
struct Task
{
    /// Execute the task. Thread index 0 is the main thread.
    virtual void Invoke(Task* task, unsigned threadIndex) = 0;
    /// Destruct.
    virtual ~Task();

    /// Data start pointer, task-specific meaning.
    void* start;
    /// Data end pointer, task-specific meaning.
    void* end;
};

/// Free function task.
struct FunctionTask : public Task
{
    typedef void (*WorkFunctionPtr)(Task*, unsigned);

    /// Construct.
    FunctionTask(WorkFunctionPtr function_, void* start_ = nullptr, void* end_ = nullptr) :
        function(function_)
    {
        start = start_;
        end = end_;
    }

    /// Execute the task. Thread index 0 is the main thread.
    void Invoke(Task* task, unsigned threadIndex) override
    {
        function(task, threadIndex);
    }

    /// Task function.
    WorkFunctionPtr function;
};

/// Member function task.
template<class T> struct MemberFunctionTask : public Task
{
    typedef void (T::* MemberWorkFunctionPtr)(Task*, unsigned);

    /// Construct.
    MemberFunctionTask(T* object_, MemberWorkFunctionPtr function_, void* start_ = nullptr, void* end_ = nullptr) :
        object(object_),
        function(function_)
    {
        start = start_;
        end = end_;
    }


    /// Execute the task. Thread index 0 is the main thread.
    void Invoke(Task* task, unsigned threadIndex) override
    {
        (object->*function)(task, threadIndex);
    }

    /// Object instance.
    T* object;
    /// Task member function.
    MemberWorkFunctionPtr function;
};

/// Worker thread subsystem for dividing tasks between CPU cores.
class WorkQueue : public Object
{
    OBJECT(WorkQueue);

public:
    /// Create with specified amount of threads including the main thread. 1 to use just the main thread. 0 to guess a suitable amount of threads from CPU core count.
    WorkQueue(unsigned numThreads);
    /// Destruct. Stop worker threads.
    ~WorkQueue();

    /// Queue a task for execution. If no threads, completes immediately in the main thread.
    void QueueTask(Task* task);
    /// Complete all currently queued tasks. To be called only from the main thread.
    void Complete();

    /// Return number of execution threads including the main thread.
    unsigned NumThreads() const { return (unsigned)threads.size() + 1; }

    /// Return thread index when outside of a work function.
    static unsigned ThreadIndex() { return threadIndex; }

private:
    /// Worker thread function.
    void WorkerLoop(unsigned threadIndex);

    /// Mutex for the work queue.
    std::mutex queueMutex;
    /// Condition variable to wake up workers.
    std::condition_variable signal;
    /// Exit flag.
    volatile bool shouldExit;
    /// Task queue.
    std::queue<Task*> tasks;
    /// Worker threads.
    std::vector<std::thread> threads;
    /// Amount of queued tasks. Used to check for completion.
    std::atomic<int> numPendingTasks;

    /// Thread index for queries outside the work functions.
    static thread_local unsigned threadIndex;
};
