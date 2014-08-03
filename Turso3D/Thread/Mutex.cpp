// For conditions of distribution and use, see copyright notice in License.txt

#include "Mutex.h"

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "../Debug/DebugNew.h"

namespace Turso3D
{

#ifdef WIN32
Mutex::Mutex() :
    handle(new CRITICAL_SECTION)
{
    InitializeCriticalSection((CRITICAL_SECTION*)handle);
}

Mutex::~Mutex()
{
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)handle;
    DeleteCriticalSection(cs);
    delete cs;
    handle = 0;
}

void Mutex::Acquire()
{
    EnterCriticalSection((CRITICAL_SECTION*)handle);
}

void Mutex::Release()
{
    LeaveCriticalSection((CRITICAL_SECTION*)handle);
}
#else
Mutex::Mutex() :
    handle(new pthread_mutex_t)
{
    pthread_mutex_t* m = (pthread_mutex_t*)handle;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &attr);
}

Mutex::~Mutex()
{
    pthread_mutex_t* m = (pthread_mutex_t*)handle;
    pthread_mutex_destroy(m);
    delete m;
    handle = 0;
}

void Mutex::Acquire()
{
    pthread_mutex_lock((pthread_mutex_t*)handle);
}

void Mutex::Release()
{
    pthread_mutex_unlock((pthread_mutex_t*)handle);
}
#endif

MutexLock::MutexLock(Mutex& mutex_) :
    mutex(mutex_)
{
    mutex.Acquire();
}

MutexLock::~MutexLock()
{
    mutex.Release();
}

}
