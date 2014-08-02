// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

namespace Turso3D
{

/// Operating system mutual exclusion primitive.
class TURSO3D_API Mutex
{
public:
    /// Construct.
    Mutex();
    /// Destruct.
    ~Mutex();
    
    /// Acquire the mutex. Block if already acquired.
    void Acquire();
    /// Release the mutex.
    void Release();
    
private:
    /// Mutex handle.
    void* handle;
};

/// Lock that automatically acquires and releases a mutex.
class TURSO3D_API MutexLock
{
public:
    /// Construct and acquire the mutex.
    MutexLock(Mutex& mutex);
    /// Destruct. Release the mutex.
    ~MutexLock();
    
private:
    /// Prevent copy construction.
    MutexLock(const MutexLock& rhs);
    /// Prevent assignment.
    MutexLock& operator = (const MutexLock& rhs);
    
    /// Mutex reference.
    Mutex& mutex;
};

}
