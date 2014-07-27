// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <cassert>
#include <cstddef>

namespace Turso3D
{

/// Pointer which takes ownership of an object and deletes it when the pointer goes out of scope. Ownership can be transferred to another pointer, in which case the source pointer becomes null.
template <class T> class AutoPtr
{
public:
    /// Construct a null pointer.
    AutoPtr() :
        ptr(0)
    {
    }

    /// Construct and take ownership of the object.
    AutoPtr(T* rhs) :
       ptr(rhs)
    {
    }

    /// Copy-construct. The ownership is transferred, making the source pointer null.
    AutoPtr(AutoPtr<T>& rhs) :
        ptr(rhs.ptr)
    {
        rhs.ptr = 0;
    }

    /// Destruct. Delete the object pointed to.
    ~AutoPtr()
    {
        delete ptr;
    }

    /// Assign from a pointer (transfer ownership). The source pointer becomes null.
    AutoPtr<T>& operator = (AutoPtr<T>& rhs)
    {
        delete ptr;
        ptr = rhs.ptr;
        rhs.ptr = 0;
        return *this;
    }

    /// Assign a new object and delete the old if any.
    AutoPtr<T>& operator = (T* rhs)
    {
        delete ptr;
        ptr = rhs;
        return *this;
    }

    /// Detach the object from the pointer without destroying it and return it. The pointer becomes null.
    T* Detach()
    {
        T* ret = ptr;
        ptr = 0;
        return ret;
    }

    /// Reset to null. Destroys the object.
    void Reset()
    {
        *this = 0;
    }
    
    /// Point to the object.
    T* operator -> () const { assert(ptr); return ptr; }
    /// Dereference the object.
    T& operator * () const { assert(ptr); return *ptr; }
    /// Convert to the object.
    operator T* () const { return ptr; }
    /// Return the object.
    T* Get() const { return ptr; }
    /// Return whether is a null pointer.
    bool IsNull() const { return ptr == 0; }
    
private:
    /// Object pointer.
    T* ptr;
};

/// Pointer which takes ownership of an array allocated with new[] and deletes it when the pointer goes out of scope. Ownership can be transferred to another pointer, in which case the source pointer becomes null.
template <class T> class AutoArrayPtr
{
public:
    /// Construct a null pointer.
    AutoArrayPtr() :
        array(0)
    {
    }

    /// Construct and take ownership of the array.
    AutoArrayPtr(T* rhs) :
       array(rhs)
    {
    }

    /// Copy-construct. The ownership is transferred, making the source pointer null.
    AutoArrayPtr(AutoArrayPtr<T>& rhs) :
        array(rhs.array)
    {
        rhs.array = 0;
    }

    /// Destruct. Delete the array pointed to.
    ~AutoArrayPtr()
    {
        delete[] array;
    }

    /// Assign from a pointer (transfer ownership). The source pointer becomes null.
    AutoArrayPtr<T>& operator = (AutoArrayPtr<T>& rhs)
    {
        delete array;
        array = rhs.array;
        rhs.array = 0;
        return *this;
    }

    /// Assign a new array and delete the old if any.
    AutoArrayPtr<T>& operator = (T* rhs)
    {
        delete array;
        array = rhs;
        return *this;
    }

    /// Detach the array from the pointer without destroying it and return it. The pointer becomes null.
    T* Detach()
    {
        T* ret = array;
        array = 0;
        return ret;
    }

    /// Reset to null. Destroys the array.
    void Reset()
    {
        *this = 0;
    }

    /// Point to the array.
    T* operator -> () const { assert(array); return array; }
    /// Dereference the array.
    T& operator * () const { assert(array); return *array; }
    /// Index the array.
    T& operator [] (size_t index) { assert(array); return array[index]; }
    /// Const-index the array.
    const T& operator [] (size_t index) const { assert(array); return array[index]; }
    /// Convert to the array.
    operator T* () const { return array; }
    /// Return the array.
    T* Get() const { return array; }
    /// Return whether is a null pointer.
    bool IsNull() const { return array == 0; }
    
private:
    /// Array pointer.
    T* array;
};

/// Base class for intrusively reference counted objects that can be pointed to with a SharedPtr. These are not copy-constructible and not assignable.
class RefCounted
{
public:
    /// Construct. Initialize the reference count to zero.
    RefCounted() :
        refCount(0)
    {
    }
    
    /// Destruct. Verify that the reference count is zero.
    ~RefCounted()
    {
        assert(refCount == 0);
    }
    
    /// Add a reference.
    void AddRef()
    {
        ++refCount;
    }
    
    /// Remove a reference. Delete the object if was the last reference.
    void ReleaseRef()
    {
        assert(refCount > 0);
        --refCount;
        if (!refCount)
            delete this;
    }
    
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
    
    /// Construct with an object pointer.
    SharedPtr(T* rhs) :
        ptr(0)
    {
        *this = rhs;
    }
    
    /// Copy-construct.
    SharedPtr(const SharedPtr<T>& rhs) :
        ptr(0)
    {
        *this = rhs;
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
    
    /// Assign another shared pointer.
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
    
    /// Test for equality: points to the same object.
    bool operator == (const SharedPtr<T>& rhs) const { return ptr == rhs.ptr; }
    /// Test for inequality.
    bool operator != (const SharedPtr<T>& rhs) const { return !(*this == rhs); }
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

/// Base class for objects that can be pointed to with a WeakPtr. These are not copy-constructible and not assignable.
class WeakRefCounted
{
public:
    /// Construct. The reference count is not allocated yet; it will be allocated on demand.
    WeakRefCounted() :
        refCount(0)
    {
    }
    
    /// Destruct. If no weak references, destroy also the reference count, else mark it expired.
    ~WeakRefCounted()
    {
        if (refCount)
        {
            if (*refCount == 0)
                delete refCount;
            else
                *refCount |= 0x80000000;
        }
    }
    
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
    
    /// Construct with an object pointer.
    WeakPtr(T* rhs) :
        ptr(0),
        refCount(0)
    {
        *this = rhs;
    }
    
    /// Copy-construct.
    WeakPtr(const WeakPtr<T>& rhs) :
        ptr(0),
        refCount(0)
    {
        *this = rhs;
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
            if (*refCount == 0x80000000)
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
    
    /// Test for equality: points to the same object and reference count.
    bool operator == (const WeakPtr<T>& rhs) const { return ptr == rhs.ptr && refCount == rhs.refCount; }
    /// Test for inequality.
    bool operator != (const WeakPtr<T>& rhs) const { return !(*this == rhs); }
    /// Point to the object.
    T* operator -> () const { T* ret = Get(); assert(ret); return ret; }
    /// Dereference the object.
    T& operator * () const { T* ret = Get(); assert(ret); return ret; }
    /// Convert to the object.
    operator T* () const { return Get(); }
    
    /// Return the object or null if it has been destroyed.
    T* Get() const
    {
        if (refCount && (*refCount & 0x80000000) == 0)
            return ptr;
        else
            return 0;
    }

    /// Return the number of weak references to the object.
    unsigned WeakRefs() const { return refCount ? *refCount & 0x7fffffff : 0; }
    /// Return whether is a null pointer.
    bool IsNull() const { return ptr == 0; }
    /// Return whether the object has been destroyed. Returns false if the pointer is null.
    bool IsExpired() const { return refCount && *refCount & 0x80000000; }
    
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
