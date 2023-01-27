// For conditions of distribution and use, see copyright notice in License.txt

#include "Timer.h"

#include <ctime>

#ifdef _WIN32
#include <Windows.h>
#include <MMSystem.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

/// \cond PRIVATE
class TimerInitializer
{
public:
    TimerInitializer()
    {
        HiresTimer::Initialize();
        #ifdef _WIN32
        timeBeginPeriod(1);
        #endif
    }

    ~TimerInitializer()
    {
        #ifdef _WIN32
        timeEndPeriod(1);
        #endif
    }
};
/// \endcond

static TimerInitializer initializer;

bool HiresTimer::supported = false;
long long HiresTimer::frequency = 1000;

Timer::Timer()
{
    Reset();
}

unsigned Timer::ElapsedMSec()
{
    #ifdef _WIN32
    unsigned currentTime = timeGetTime();
    #else
    struct timeval time;
    gettimeofday(&time, 0);
    unsigned currentTime = time.tv_sec * 1000 + time.tv_usec / 1000;
    #endif
    
    return currentTime - startTime;
}

void Timer::Reset()
{
    #ifdef _WIN32
    startTime = timeGetTime();
    #else
    struct timeval time;
    gettimeofday(&time, 0);
    startTime = time.tv_sec * 1000 + time.tv_usec / 1000;
    #endif
}

HiresTimer::HiresTimer()
{
    Reset();
}

long long HiresTimer::ElapsedUSec()
{
    long long currentTime;
    
    #ifdef _WIN32
    if (supported)
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        currentTime = counter.QuadPart;
    }
    else
        currentTime = timeGetTime();
    #else
    struct timeval time;
    gettimeofday(&time, 0);
    currentTime = time.tv_sec * 1000000LL + time.tv_usec;
    #endif
    
    long long elapsedTime = currentTime - startTime;
    
    // Correct for possible weirdness with changing internal frequency
    if (elapsedTime < 0)
        elapsedTime = 0;
    
    return (elapsedTime * 1000000LL) / frequency;
}

void HiresTimer::Reset()
{
    #ifdef _WIN32
    if (supported)
    {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        startTime = counter.QuadPart;
    }
    else
        startTime = timeGetTime();
    #else
    struct timeval time;
    gettimeofday(&time, 0);
    startTime = time.tv_sec * 1000000LL + time.tv_usec;
    #endif
}

void HiresTimer::Initialize()
{
    #ifdef _WIN32
    LARGE_INTEGER frequency_;
    if (QueryPerformanceFrequency(&frequency_))
    {
        frequency = frequency_.QuadPart;
        supported = true;
    }
    #else
    frequency = 1000000;
    supported = true;
    #endif
}
