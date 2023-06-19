// For conditions of distribution and use, see copyright notice in License.txt

#include "ThreadUtils.h"
#include "WorkQueue.h"

#include <tracy/Tracy.hpp>

thread_local unsigned WorkQueue::threadIndex = 0;

Task::Task()
{
    numDependencies.store(0);
}

Task::~Task()
{
}

WorkQueue::WorkQueue(unsigned numThreads) :
    shouldExit(false)
{
    RegisterSubsystem(this);

    numQueuedTasks.store(0);
    numPendingTasks.store(0);

    if (numThreads == 0)
    {
        numThreads = CPUCount();
        // Avoid completely excessive core count
        if (numThreads > 16)
            numThreads = 16;
    }

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
    assert(task->numDependencies.load() == 0);

    if (threads.size())
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.push(task);
        }

        numQueuedTasks.fetch_add(1);
        numPendingTasks.fetch_add(1);

        signal.notify_one();
    }
    else
    {
        // If no threads, execute directly
        CompleteTask(task, 0);
    }
}

void WorkQueue::QueueTasks(size_t count, Task** tasks_)
{
    if (threads.size())
    {
        ZoneScoped;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (size_t i = 0; i < count; ++i)
            {
                assert(tasks_[i]);
                assert(tasks_[i]->numDependencies.load() == 0);
                tasks.push(tasks_[i]);
            }
        }

        numQueuedTasks.fetch_add((int)count);
        numPendingTasks.fetch_add((int)count);

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
        {
            assert(tasks_[i]);
            assert(tasks_[i]->numDependencies.load() == 0);
            CompleteTask(tasks_[i], 0);
        }
    }
}

void WorkQueue::AddDependency(Task* task, Task* dependency)
{
    assert(task);
    assert(dependency);

    dependency->dependentTasks.push_back(task);

    // If this is the first dependency added, increment the global pending task counter so we know to wait for the dependent task in Complete().
    if (task->numDependencies.fetch_add(1) == 0)
        numPendingTasks.fetch_add(1);
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
        }

        numQueuedTasks.fetch_add(-1);
        CompleteTask(task, 0);
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
    }

    numQueuedTasks.fetch_add(-1);
    CompleteTask(task, 0);

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
        }

        numQueuedTasks.fetch_add(-1);
        CompleteTask(task, threadIndex_);
    }
}

void WorkQueue::CompleteTask(Task* task, unsigned threadIndex_)
{
    task->Complete(threadIndex_);

    if (task->dependentTasks.size())
    {
        // Queue dependent tasks now if no more dependencies left
        for (auto it = task->dependentTasks.begin(); it != task->dependentTasks.end(); ++it)
        {
            Task* dependentTask = *it;

            if (dependentTask->numDependencies.fetch_add(-1) == 1)
            {
                if (threads.size())
                {
                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        tasks.push(dependentTask);
                    }

                    // Note: numPendingTasks counter was already incremented when adding the first dependency, do not do again here
                    numQueuedTasks.fetch_add(1);

                    signal.notify_one();
                }
                else
                {
                    // If no threads, execute directly
                    CompleteTask(dependentTask, 0);
                }
            }
        }

        task->dependentTasks.clear();
    }

    // Decrement pending task counter last, so that WorkQueue::Complete() will also wait for the potentially added dependent tasks
    numPendingTasks.fetch_add(-1);
}
