// For conditions of distribution and use, see copyright notice in License.txt

#include "ThreadUtils.h"
#include "WorkQueue.h"

#include <tracy/Tracy.hpp>

thread_local unsigned WorkQueue::threadIndex = 0;

Task::~Task()
{
}

WorkQueue::WorkQueue(unsigned numThreads) :
    shouldExit(false)
{
    RegisterSubsystem(this);

    numPendingTasks.store(0);

    if (numThreads == 0)
    {
        numThreads = CPUCount();
        // Avoid completely excessive core count
        if (numThreads > 16)
            numThreads = 16;
    }

    if (numThreads > 0)
    {
        exitTask = new MemberFunctionTask<WorkQueue>(this, &WorkQueue::ExitWorkerThreadWork);
        for (unsigned  i = 0; i < numThreads - 1; ++i)
            threads.push_back(std::thread(&WorkQueue::WorkerLoop, this, i + 1));
    }
}

WorkQueue::~WorkQueue()
{
    if (!threads.size())
        return;

    // Signal exit and wait for threads to finish
    shouldExit = true;
    for (size_t i = 0; i < threads.size(); ++i)
        QueueTask(exitTask);

    for (auto it = threads.begin(); it != threads.end(); ++it)
        it->join();
}

void WorkQueue::QueueTask(Task* task)
{
    ZoneScoped;

    if (threads.size())
    {
        numPendingTasks.fetch_add(1);
        taskQueue.enqueue(task);
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
        numPendingTasks.fetch_add((int)count);
        taskQueue.enqueue_bulk(tasks_, count);
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

        // If have still tasks, execute them in the main thread
        Task* task;
        if (taskQueue.try_dequeue(task))
        {
            task->Invoke(task, 0);
            numPendingTasks.fetch_add(-1);
        }
    }
}

void WorkQueue::WorkerLoop(unsigned threadIndex_)
{
    WorkQueue::threadIndex = threadIndex_;

    while (!shouldExit)
    {
        Task* task;
        taskQueue.wait_dequeue(task);
        task->Invoke(task, threadIndex_);
        numPendingTasks.fetch_add(-1);
    }
}

void WorkQueue::ExitWorkerThreadWork(Task*, unsigned)
{
}
