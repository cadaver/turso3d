// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

#include <cassert>

namespace Turso3D
{

/// Base class for intrusively reference counted objects that can be pointed to with a SharedPtr. These are not copy-constructible and not assignable.
class TURSO3D_API RefCounted
{
public:
    /// Construct. Initialize the reference count to zero.
    RefCounted();
    
    /// Destruct. Verify that the reference count is zero.
    virtual ~RefCounted();
    
    /// Add a reference.
    void AddRef();
    /// Remove a reference. Delete the object if was the last reference.
    void ReleaseRef();

    /// Return the number of references.
    unsigned Refs() const { return refCount; }
    
private:
    /// Prevent copy construction.
    RefCounted(const RefCounted& rhs);
    /// Prevent assignment.
    RefCounted& operator = (const RefCounted& rhs);
    
    /// Reference count.
    unsigned refCount;
};

/// Pointer which refers to a RefCounted subclass and has shared ownership of it.
template <class T> class SharedPtr
{
public:
    /// Construct a null pointer.
    SharedPtr() :
        ptr(0)
    {
    }
    
    /// Copy-construct.
    SharedPtr(const SharedPtr<T>& ptr_) :
        ptr(0)
    {
        *this = ptr_;
    }

    /// Construct with an object pointer.
    SharedPtr(T* ptr_) :
        ptr(0)
    {
        *this = ptr_;
    }
    
    /// Destruct. Release the object reference and destroy the object if was the last reference.
    ~SharedPtr()
    {
        Reset();
    }
    
    /// Assign an object.
    SharedPtr<T>& operator = (T* rhs)
    {
        Reset();
        ptr = rhs;
        if (ptr)
            ptr->AddRef();
        return *this;
    }
    
    /// Assign from another shared pointer.
    SharedPtr<T>& operator = (const SharedPtr<T>& rhs)
    {
        Reset();
        ptr = rhs.ptr;
        if (ptr)
            ptr->AddRef();
        return *this;
    }
    
    /// Release the object reference and reset to null. Destroy the object if was the last reference.
    void Reset()
    {
        if (ptr)
        {
            ptr->ReleaseRef();
            ptr = 0;
        }
    }
    
    /// Perform a static cast from a SharedPtr of another type.
    template <class U> void StaticCast(const SharedPtr<U>& rhs)
    {
        *this = static_cast<T*>(rhs.ptr);
    }

    /// Perform a dynamic cast from a WeakPtr of another type.
    template <class U> void DynamicCast(const SharedPtr<U>& rhs)
    {
        Reset();
        T* rhsObject = dynamic_cast<T*>(rhs.ptr);
        if (rhsObject)
            *this = rhsObject;
    }
    
    /// Test for equality with another shared pointer.
    bool operator == (const SharedPtr<T>& rhs) const { return ptr == rhs.ptr; }
    /// Test for equality with an object pointer.
    bool operator == (T* rhs) const { return ptr == rhs; }
    /// Test for inequality with another shared pointer.
    bool operator != (const SharedPtr<T>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with an object pointer.
    bool operator != (T* rhs) const { return !(*this == rhs); }
    /// Point to the object.
    T* operator -> () const { assert(ptr); return ptr; }
    /// Dereference the object.
    T& operator * () const { assert(ptr); return ptr; }
    /// Convert to the object.
    operator T* () const { return ptr; }
    
    /// Return the object.
    T* Get() const { return ptr; }
    /// Return the number of references to the object.
    unsigned Refs() const { return ptr ? ptr->Refs() : 0; }
    /// Return whether is a null pointer.
    bool IsNull() const { return ptr == 0; }
    
private:
    /// Object pointer.
    T* ptr;
};

/// Perform a static cast between shared pointers of two types.
template <class T, class U> SharedPtr<T> StaticCast(const SharedPtr<U>& rhs)
{
    SharedPtr<T> ret;
    ret.StaticCast(rhs);
    return ret;
}

/// Perform a dynamic cast between shared pointers of two types.
template <class T, class U> SharedPtr<T> DynamicCast(const SharedPtr<U>& rhs)
{
    SharedPtr<T> ret;
    ret.DynamicCast(rhs);
    return ret;
}

}
