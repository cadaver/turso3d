// For conditions of distribution and use, see copyright notice in License.txt

#include "Condition.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32
Condition::Condition() :
    event(nullptr)
{
    event = CreateEvent(0, FALSE, FALSE, 0);
}

Condition::~Condition()
{
    CloseHandle((HANDLE)event);
    event = nullptr;
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
    event = nullptr;
    mutex = nullptr;
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
