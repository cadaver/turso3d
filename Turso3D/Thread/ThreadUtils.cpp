#include "ThreadUtils.h"

#include <thread>

std::thread::id mainThreadId = std::this_thread::get_id();

bool IsMainThread()
{
    return std::this_thread::get_id() == mainThreadId;
}

unsigned CPUCount()
{
    return std::thread::hardware_concurrency();
}