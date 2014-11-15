// For conditions of distribution and use, see copyright notice in License.txt

#include "Thread.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include "../Debug/DebugNew.h"

namespace Turso3D
{

#ifdef _WIN32
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
    pthread_exit(nullptr);
    return 0;
}
#endif

ThreadID Thread::mainThreadID = Thread::CurrentThreadID();

Thread::Thread() :
    handle(nullptr),
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
    #ifdef _WIN32
    handle = CreateThread(nullptr, 0, ThreadFunctionStatic, this, 0, nullptr);
    #else
    handle = new pthread_t;
    pthread_attr_t type;
    pthread_attr_init(&type);
    pthread_attr_setdetachstate(&type, PTHREAD_CREATE_JOINABLE);
    pthread_create((pthread_t*)handle, &type, ThreadFunctionStatic, this);
    #endif
    return handle != nullptr;
}

void Thread::Stop()
{
    // Check if already stopped
    if (!handle)
        return;
    
    shouldRun = false;
    #ifdef _WIN32
    WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
    #else
    pthread_t* t = (pthread_t*)handle;
    if (t)
        pthread_join(*t, 0);
    delete t;
    #endif
    handle = nullptr;
}

void Thread::SetPriority(int priority)
{
    #ifdef _WIN32
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
    #ifdef _WIN32
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
    #ifdef _WIN32
    return GetCurrentThreadId();
    #else
    return pthread_self();
    #endif
}

bool Thread::IsMainThread()
{
    return CurrentThreadID() == mainThreadID;
}

}
