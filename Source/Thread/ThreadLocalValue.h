// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#ifndef WIN32
#include <pthread.h>
#endif

#ifdef _WIN32
typedef unsigned TLSKeyID;
#else
typedef pthread_key_t TLSKeyID;
#endif

/// %Thread local storage value.
class ThreadLocalValue
{
public:
    /// Construct.
    ThreadLocalValue();
    
    /// Destruct.
    ~ThreadLocalValue();

    /// Set the value.
    void SetValue(void* value);

    /// Return the value.
    void* Value() const;
    
    /// Return whether was successfully allocated. Returns false when the OS resources for thread local values have been exhausted.
    bool Valid() const { return valid; }

private:
    /// Key used by the OS to identify the value.
    TLSKeyID key;
    /// Valid flag.
    bool valid;
};
