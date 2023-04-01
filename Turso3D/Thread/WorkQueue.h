// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/Object.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

/// %Task for execution by worker threads.
struct Task
{
    /// Default-construct.
    Task();
    /// Destruct.
    virtual ~Task();

    /// Call the work function. Thread index 0 is the main thread.
    virtual void Complete(unsigned threadIndex) = 0;

    /// Add a task depended on. These need to be added for each execution. Adding dependencies is not threadsafe, so should be done before queuing.
    void AddDependency(Task* task)
    {
        task->dependentTasks.push_back(this);
        numDependencies.fetch_add(1);
    }

    /// Dependent tasks.
    std::vector<Task*> dependentTasks;
    /// Dependency counter. Once zero, this task will be automatically queue itself.
    std::atomic<int> numDependencies;
};

/// Free function task.
struct FunctionTask : public Task
{
    typedef void (*WorkFunctionPtr)(Task*, unsigned);

    /// Construct.
    FunctionTask(WorkFunctionPtr function_) :
        function(function_)
    {
    }

    /// Call the work function. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override
    {
        function(this, threadIndex);
    }

    /// Task function.
    WorkFunctionPtr function;
};

/// Member function task.
template<class T> struct MemberFunctionTask : public Task
{
    typedef void (T::* MemberWorkFunctionPtr)(Task*, unsigned);

    /// Construct.
    MemberFunctionTask(T* object_, MemberWorkFunctionPtr function_) :
        object(object_),
        function(function_)
    {
    }

    /// Call the work function. Thread index 0 is the main thread.
    void Complete(unsigned threadIndex) override
    {
        (object->*function)(this, threadIndex);
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

    /// Queue a task for execution. If no threads, completes immediately in the main thread. Do not queue tasks with dependencies, they will instead queue themselves.
    void QueueTask(Task* task);
    /// Queue several tasks execution. If no threads, completes immediately in the main thread. Do not queue tasks with dependencies, they will instead queue themselves.
    void QueueTasks(size_t count, Task** tasks);
    /// Complete all currently queued tasks. To be called only from the main thread.
    void Complete();
    /// Execute a task from the queue if available, then return. To be called only from the main thread. Return true if a task was executed.
    bool TryComplete();

    /// Return number of execution threads including the main thread.
    unsigned NumThreads() const { return (unsigned)threads.size() + 1; }

    /// Return thread index when outside of a work function.
    static unsigned ThreadIndex() { return threadIndex; }

private:
    /// Worker thread function.
    void WorkerLoop(unsigned threadIndex);
    /// Complete a task by calling its work function and signal dependents.
    void CompleteTask(Task*, unsigned threadIndex);

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
    /// Amount of tasks in queue.
    std::atomic<int> numQueuedTasks;
    /// Amount of queued tasks. Used to check for completion.
    std::atomic<int> numPendingTasks;

    /// Thread index for queries outside the work functions.
    static thread_local unsigned threadIndex;
};
