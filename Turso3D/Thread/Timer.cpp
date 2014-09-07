// For conditions of distribution and use, see copyright notice in License.txt

#include "Timer.h"

#include <ctime>

#ifdef WIN32
#include <Windows.h>
#include <MMSystem.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include "../Debug/DebugNew.h"

namespace Turso3D
{

/// Static initializer for timers.
class TimerInitializer
{
public:
    /// Construct. Initialize the high-res timer and set maximum resolution for the low-res timer (Windows only.)
    TimerInitializer()
    {
        HiresTimer::Initialize();
        #ifdef WIN32
        timeBeginPeriod(1);
        #endif
    }

    /// Destruct. Restore default resolution of the low-res timer (Windows only.)
    ~TimerInitializer()
    {
        #ifdef WIN32
        timeEndPeriod(1);
        #endif
    }
};

static TimerInitializer initializer;

bool HiresTimer::supported = false;
long long HiresTimer::frequency = 1000;

String TimeStamp()
{
    time_t sysTime;
    time(&sysTime);
    const char* dateTime = ctime(&sysTime);
    return String(dateTime).Replaced("\n", "");
}

Timer::Timer()
{
    Reset();
}

unsigned Timer::ElapsedMSec(bool reset)
{
    #ifdef WIN32
    unsigned currentTime = timeGetTime();
    #else
    struct timeval time;
    gettimeofday(&time, NULL);
    unsigned currentTime = time.tv_sec * 1000 + time.tv_usec / 1000;
    #endif
    
    unsigned elapsedTime = currentTime - startTime;
    if (reset)
        startTime = currentTime;
    
    return elapsedTime;
}

void Timer::Reset()
{
    #ifdef WIN32
    startTime = timeGetTime();
    #else
    struct timeval time;
    gettimeofday(&time, NULL);
    startTime = time.tv_sec * 1000 + time.tv_usec / 1000;
    #endif
}

HiresTimer::HiresTimer()
{
    Reset();
}

long long HiresTimer::ElapsedUSec(bool reset)
{
    long long currentTime;
    
    #ifdef WIN32
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
    gettimeofday(&time, NULL);
    currentTime = time.tv_sec * 1000000LL + time.tv_usec;
    #endif
    
    long long elapsedTime = currentTime - startTime;
    
    // Correct for possible weirdness with changing internal frequency
    if (elapsedTime < 0)
        elapsedTime = 0;
    
    if (reset)
        startTime = currentTime;
    
    return (elapsedTime * 1000000LL) / frequency;
}

void HiresTimer::Reset()
{
    #ifdef WIN32
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
    gettimeofday(&time, NULL);
    startTime = time.tv_sec * 1000000LL + time.tv_usec;
    #endif
}

void HiresTimer::Initialize()
{
    #ifdef WIN32
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

}
