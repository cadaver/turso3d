// For conditions of distribution and use, see copyright notice in License.txt

#include "ThreadUtils.h"
#include "WorkQueue.h"

#include <tracy/Tracy.hpp>

thread_local unsigned WorkQueue::threadIndex = 0;

Task::Task()
{
    dependencyCounter.store(0);
}

Task::~Task()
{
}

void Task::CompleteDependency()
{
    if (dependencyCounter.fetch_add(-1) == 1)
        Object::Subsystem<WorkQueue>()->QueueTask(this);
}

WorkQueue::WorkQueue(unsigned numThreads) :
    shouldExit(false)
{
    RegisterSubsystem(this);

    numQueuedTasks.store(0);
    numPendingTasks.store(0);

    if (numThreads == 0)
        numThreads = CPUCount();
    // Avoid completely excessive core count
    if (numThreads > 16)
        numThreads = 16;

    for (unsigned  i = 0; i < numThreads - 1; ++i)
        threads.push_back(std::thread(&WorkQueue::WorkerLoop, this, i + 1));
}

WorkQueue::~WorkQueue()
{
    if (!threads.size())
        return;

    // Signal exit and wait for threads to finish
    shouldExit = true;

    signal.notify_all();
    for (auto it = threads.begin(); it != threads.end(); ++it)
        it->join();
}

void WorkQueue::QueueTask(Task* task)
{
    assert(task);

    if (threads.size())
    {
        assert(task->dependencyCounter.load() == 0);

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.push(task);
            numQueuedTasks.fetch_add(1);
            numPendingTasks.fetch_add(1);
        }

        signal.notify_one();
    }
    else
    {
        // If no threads, execute directly
        task->Invoke(task, 0);
    }
}

void WorkQueue::QueueTasks(size_t count, Task** tasks_)
{
    ZoneScoped;

    if (threads.size())
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (size_t i = 0; i < count; ++i)
            {
                assert(tasks_[i]->dependencyCounter.load() == 0);
                tasks.push(tasks_[i]);
            }
            numQueuedTasks.fetch_add((int)count);
            numPendingTasks.fetch_add((int)count);
        }

        if (count >= threads.size())
            signal.notify_all();
        else
        {
            for (size_t i = 0; i < count; ++i)
                signal.notify_one();
        }
    }
    else
    {
        // If no threads, execute directly
        for (size_t i = 0; i < count; ++i)
            tasks_[i]->Invoke(tasks_[i], 0);
    }
}

void WorkQueue::Complete()
{
    ZoneScoped;
    
    if (!threads.size())
        return;

    for (;;)
    {
        if (!numPendingTasks.load())
            break;

        // Avoid locking the queue mutex if do not have tasks in queue, just wait for the workers to finish
        if (!numQueuedTasks.load())
            continue;

        // Otherwise if have still tasks, execute them in the main thread
        Task* task;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!tasks.size())
                continue;

            task = tasks.front();
            tasks.pop();
            numQueuedTasks.fetch_add(-1);
        }

        task->Invoke(task, 0);
        numPendingTasks.fetch_add(-1);
    }
}

bool WorkQueue::TryComplete()
{
    if (!threads.size() || !numPendingTasks.load() || !numQueuedTasks.load())
        return false;

    Task* task;

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (!tasks.size())
            return false;

        task = tasks.front();
        tasks.pop();
        numQueuedTasks.fetch_add(-1);
    }

    task->Invoke(task, 0);
    numPendingTasks.fetch_add(-1);

    return true;
}

void WorkQueue::WorkerLoop(unsigned threadIndex_)
{
    WorkQueue::threadIndex = threadIndex_;

    for (;;)
    {
        Task* task;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            signal.wait(lock, [this]
            {
                return !tasks.empty() || shouldExit;
            });

            if (shouldExit)
                break;

            task = tasks.front();
            tasks.pop();
            numQueuedTasks.fetch_add(-1);
        }

        // Note: if task has dependents, they will be queued during Invoke(), so WorkQueue::Complete() will not exit erroneously early
        task->Invoke(task, threadIndex_);
        numPendingTasks.fetch_add(-1);
    }
}
