// For conditions of distribution and use, see copyright notice in License.txt

#include "ThreadUtils.h"
#include "WorkQueue.h"

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
        // If have excess cores, try to leave some free e.g. for the graphics driver
        if (numThreads >= 8)
            numThreads /= 2;
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
    if (threads.size())
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.push(task);
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

void WorkQueue::QueueTasks(const std::vector<Task*>& tasks_)
{
    if (!tasks_.size())
        return;

    if (threads.size())
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (auto it = tasks_.begin(); it != tasks_.end(); ++it)
                tasks.push(*it);
            numPendingTasks.fetch_add((int)tasks_.size());
        }

        signal.notify_all();
    }
    else
    {
        // If no threads, execute directly
        for (auto it = tasks_.begin(); it != tasks_.end(); ++it)
            (*it)->Invoke(*it, 0);
    }
}

void WorkQueue::QueueTasks(const std::vector<AutoPtr<Task> >& tasks_)
{
    if (!tasks_.size())
        return;

    if (threads.size())
    {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (auto it = tasks_.begin(); it != tasks_.end(); ++it)
                tasks.push(it->Get());
            numPendingTasks.fetch_add((int)tasks_.size());
        }

        signal.notify_all();
    }
    else
    {
        // If no threads, execute directly
        for (auto it = tasks_.begin(); it != tasks_.end(); ++it)
            it->Get()->Invoke(*it, 0);
    }
}


void WorkQueue::Complete()
{
    if (!threads.size())
        return;

    for (;;)
    {
        size_t tasksLeft = numPendingTasks.load();
        if (!tasksLeft)
            break;

        Task* task;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!tasks.size())
                continue;

            task = tasks.front();
            tasks.pop();
        }

        task->Invoke(task, 0);
        numPendingTasks.fetch_add(-1);
    }
}

void WorkQueue::WorkerLoop(unsigned threadIndex)
{
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

        task->Invoke(task, threadIndex);
        numPendingTasks.fetch_add(-1);
    }
}
