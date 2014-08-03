// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

#include <cassert>

namespace Turso3D
{

/// Base class for objects that can be pointed to with a WeakPtr. These are not copy-constructible and not assignable.
class TURSO3D_API WeakRefCounted
{
public:
    /// The highest bit of the reference count denotes an expired object.
    static const unsigned EXPIRED = 0x80000000;
    /// The rest of the bits are used for the actual reference count.
    static const unsigned REFCOUNT_MASK = 0x7fffffff;
    
    /// Construct. The reference count is not allocated yet; it will be allocated on demand.
    WeakRefCounted();
    
    /// Destruct. If no weak references, destroy also the reference count, else mark it expired.
    virtual ~WeakRefCounted();

    /// Return the number of weak references.
    unsigned WeakRefs() const { return refCount ? *refCount : 0; }
    /// Return pointer to the weak reference count. Allocate if not allocated yet. Called by WeakPtr.
    unsigned* WeakRefCountPtr();
    
private:
    /// Prevent copy construction.
    WeakRefCounted(const WeakRefCounted& rhs);
    /// Prevent assignment.
    WeakRefCounted& operator = (const WeakRefCounted& rhs);
    
    /// Weak reference count, allocated on demand. The highest bit will be set when expired.
    unsigned* refCount;
};

/// Pointer which refers to a WeakRefCounted subclass to know whether the object exists and its number of weak references. Does not own the object pointed to.
template <class T> class WeakPtr
{
public:
    /// Construct a null pointer.
    WeakPtr() :
        ptr(0),
        refCount(0)
    {
    }
    
    /// Copy-construct.
    WeakPtr(const WeakPtr<T>& ptr_) :
        ptr(0),
        refCount(0)
    {
        *this = ptr_;
    }

    /// Construct with an object pointer.
    WeakPtr(T* ptr_) :
        ptr(0),
        refCount(0)
    {
        *this = ptr_;
    }
    
    /// Destruct. Release the object reference.
    ~WeakPtr()
    {
        Reset();
    }
    
    /// Assign an object.
    WeakPtr<T>& operator = (T* rhs)
    {
        Reset();
        ptr = rhs;
        refCount = rhs ? rhs->WeakRefCountPtr() : 0;
        if (refCount)
            ++(*refCount);
        return *this;
    }
    
    /// Assign another weak pointer.
    WeakPtr<T>& operator = (const WeakPtr<T>& rhs)
    {
        Reset();
        ptr = rhs.ptr;
        refCount = rhs.refCount;
        if (refCount)
            ++(*refCount);
        return *this;
    }
    
    /// Release the weak reference and reset to null.
    void Reset()
    {
        if (refCount)
        {
            --(*refCount);
            // If expired and no more weak references, destroy the reference count
            if (*refCount == WeakRefCounted::EXPIRED)
                delete refCount;
            ptr = 0;
            refCount = 0;
        }
    }
    
    /// Perform a static cast from a WeakPtr of another type.
    template <class U> void StaticCast(const WeakPtr<U>& rhs)
    {
        *this = static_cast<T*>(rhs.ptr);
    }

    /// Perform a dynamic cast from a WeakPtr of another type.
    template <class U> void DynamicCast(const WeakPtr<U>& rhs)
    {
        Reset();
        T* rhsObject = dynamic_cast<T*>(rhs.ptr);
        if (rhsObject)
            *this = rhsObject;
    }
    
    /// Test for equality with another weak pointer.
    bool operator == (const WeakPtr<T>& rhs) const { return ptr == rhs.ptr && refCount == rhs.refCount; }
    /// Test for equality with an object pointer.
    bool operator == (T* rhs) const { return ptr == rhs; }
    /// Test for inequality with another weak pointer.
    bool operator != (const WeakPtr<T>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an object pointer.
    bool operator != (T* rhs) const { return !(*this == rhs); }
    /// Point to the object.
    T* operator -> () const { T* ret = Get(); assert(ret); return ret; }
    /// Dereference the object.
    T& operator * () const { T* ret = Get(); assert(ret); return ret; }
    /// Convert to the object.
    operator T* () const { return Get(); }
    
    /// Return the object or null if it has been destroyed.
    T* Get() const
    {
        if (refCount && *refCount < WeakRefCounted::EXPIRED)
            return ptr;
        else
            return 0;
    }

    /// Return the number of weak references to the object.
    unsigned WeakRefs() const { return refCount ? *refCount & WeakRefCounted::REFCOUNT_MASK : 0; }
    /// Return whether is a null pointer.
    bool IsNull() const { return ptr == 0; }
    /// Return whether the object has been destroyed. Returns false if the pointer is null.
    bool IsExpired() const { return refCount && *refCount >= WeakRefCounted::EXPIRED; }
    
private:
    /// Object pointer.
    T* ptr;
    /// The object's weak reference count.
    unsigned* refCount;
};

/// Perform a static cast between weak pointers of two types.
template <class T, class U> WeakPtr<T> StaticCast(const WeakPtr<U>& rhs)
{
    WeakPtr<T> ret;
    ret.StaticCast(rhs);
    return ret;
}

/// Perform a dynamic cast between weak pointers of two types.
template <class T, class U> WeakPtr<T> DynamicCast(const WeakPtr<U>& rhs)
{
    WeakPtr<T> ret;
    ret.DynamicCast(rhs);
    return ret;
}

}
