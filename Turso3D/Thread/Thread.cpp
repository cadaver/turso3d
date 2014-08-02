// For conditions of distribution and use, see copyright notice in License.txt

#include "Condition.h"
#include "Mutex.h"
#include "Thread.h"
#include "ThreadLocalValue.h"

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "../Debug/DebugNew.h"

namespace Turso3D
{

#ifdef WIN32
Condition::Condition() :
    event(0)
{
    event = CreateEvent(0, FALSE, FALSE, 0);
}

Condition::~Condition()
{
    CloseHandle((HANDLE)event);
    event = 0;
}

void Condition::Set()
{
    SetEvent((HANDLE)event);
}

void Condition::Wait()
{
    WaitForSingleObject((HANDLE)event, INFINITE);
}
#else
Condition::Condition() :
    mutex(new pthread_mutex_t),
    event(new pthread_cond_t)
{
    pthread_mutex_init((pthread_mutex_t*)mutex, 0);
    pthread_cond_init((pthread_cond_t*)event, 0);
}

Condition::~Condition()
{
    pthread_cond_t* c = (pthread_cond_t*)event;
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;

    pthread_cond_destroy(c);
    pthread_mutex_destroy(m);
    delete c;
    delete m;
    event = 0;
    mutex = 0;
}

void Condition::Set()
{
    pthread_cond_signal((pthread_cond_t*)event);
}

void Condition::Wait()
{
    pthread_cond_t* c = (pthread_cond_t*)event;
    pthread_mutex_t* m = (pthread_mutex_t*)mutex;

    pthread_mutex_lock(m);
    pthread_cond_wait(c, m);
    pthread_mutex_unlock(m);
}
#endif

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

#ifdef WIN32
DWORD WINAPI ThreadFunctionStatic(void* data)
{
    Thread* thread = static_cast<Thread*>(data);
    thread->ThreadFunction();
    return 0;
}
#else
void* ThreadFunctionStatic(void* data)
{
    Thread* thread = static_cast<Thread*>(data);
    thread->ThreadFunction();
    pthread_exit((void*)0);
    return 0;
}
#endif

ThreadID Thread::mainThreadID;

Thread::Thread() :
    handle(0),
    shouldRun(false)
{
}

Thread::~Thread()
{
    Stop();
}

bool Thread::Run()
{
    // Check if already running
    if (handle)
        return false;
    
    shouldRun = true;
    #ifdef WIN32
    handle = CreateThread(0, 0, ThreadFunctionStatic, this, 0, 0);
    #else
    handle = new pthread_t;
    pthread_attr_t type;
    pthread_attr_init(&type);
    pthread_attr_setdetachstate(&type, PTHREAD_CREATE_JOINABLE);
    pthread_create((pthread_t*)handle, &type, ThreadFunctionStatic, this);
    #endif
    return handle != 0;
}

void Thread::Stop()
{
    // Check if already stopped
    if (!handle)
        return;
    
    shouldRun = false;
    #ifdef WIN32
    WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
    #else
    pthread_t* t = (pthread_t*)handle;
    if (t)
        pthread_join(*t, 0);
    delete t;
    #endif
    handle = 0;
}

void Thread::SetPriority(int priority)
{
    #ifdef WIN32
    if (handle)
        SetThreadPriority((HANDLE)handle, priority);
    #endif
    #if defined(__linux__) && !defined(ANDROID)
    pthread_t* t = (pthread_t*)handle;
    if (t)
        pthread_setschedprio(*t, priority);
    #endif
}

void Thread::Sleep(unsigned mSec)
{
    #ifdef WIN32
    ::Sleep(mSec);
    #else
    usleep(mSec * 1000);
    #endif
}

void Thread::SetMainThread()
{
    mainThreadID = CurrentThreadID();
}

ThreadID Thread::CurrentThreadID()
{
    #ifdef WIN32
    return GetCurrentThreadId();
    #else
    return pthread_self();
    #endif
}

bool Thread::IsMainThread()
{
    return CurrentThreadID() == mainThreadID;
}

ThreadLocalValue::ThreadLocalValue()
{
    #ifdef WIN32
    key = TlsAlloc();
    valid = key != TLS_OUT_OF_INDEXES;
    #else
    valid = pthread_key_create(&key, 0) == 0;
    #endif
}

ThreadLocalValue::~ThreadLocalValue()
{
    if (valid)
    {
        #ifdef WIN32
        TlsFree(key);
        #else
        pthread_key_delete(key);
        #endif
    }
}

void ThreadLocalValue::SetValue(void* value)
{
    if (valid)
    {
        #ifdef WIN32
        TlsSetValue(key, value);
        #else
        pthread_setspecific(key, value);
        #endif
    }
}

void* ThreadLocalValue::Value() const
{
    if (valid)
    {
        #ifdef WIN32
        return TlsGetValue(key);
        #else
        return pthread_getspecific(key);
        #endif
    }
    else
        return 0;
}

}
