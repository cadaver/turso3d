// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

#include <cassert>

namespace Turso3D
{

class RefCounted;
template <class T> class WeakPtr;

/// Reference count structure. Used in both intrusive and non-intrusive reference counting.
struct TURSO3D_API RefCount
{
    /// Construct with zero refcounts.
    RefCount() :
        refs(0),
        weakRefs(0),
        expired(false)
    {
    }

    /// Number of strong references. These keep the object alive.
    unsigned refs;
    /// Number of weak references.
    unsigned weakRefs;
    /// Expired flag. The object is no longer safe to access after this is set true.
    bool expired;
};

/// Base class for intrusively reference counted objects that can be pointed to with Ptr and WeakPtr. These are not copy-constructible and not assignable.
class TURSO3D_API RefCounted
{
public:
    /// Construct. The reference count is not allocated yet; it will be allocated on demand.
    RefCounted();

    /// Destruct. If no weak references, destroy also the reference count, else mark it expired.
    virtual ~RefCounted();

    /// Add a strong reference.
    void AddRef();
    /// Release a strong reference. Destroy the object when the last strong reference is gone.
    void ReleaseRef();

    /// Return the number of strong references.
    unsigned Refs() const { return refCount ? refCount->refs : 0; }
    /// Return the number of weak references.
    unsigned WeakRefs() const { return refCount ? refCount->weakRefs : 0; }
    /// Return pointer to the reference count structure. Allocate if not allocated yet. Called by Ptr & WeakPtr.
    RefCount* RefCountPtr();

private:
    /// Prevent copy construction.
    RefCounted(const RefCounted& rhs);
    /// Prevent assignment.
    RefCounted& operator = (const RefCounted& rhs);

    /// Reference count structure, allocated on demand.
    RefCount* refCount;
};

/// Pointer which holds a strong reference to a RefCounted subclass and allows shared ownership.
template <class T> class Ptr
{
public:
    /// Construct a null pointer.
    Ptr() :
        ptr(nullptr)
    {
    }
    
    /// Copy-construct.
    Ptr(const Ptr<T>& ptr_) :
        ptr(nullptr)
    {
        *this = ptr_;
    }

    /// Construct from a raw pointer.
    Ptr(T* ptr_) :
        ptr(nullptr)
    {
        *this = ptr_;
    }
    
    /// Destruct. Release the object reference and destroy the object if was the last strong reference.
    ~Ptr()
    {
        Reset();
    }
    
    /// Assign a raw pointer.
    Ptr<T>& operator = (T* rhs)
    {
        if (*this == rhs)
            return *this;

        Reset();
        ptr = rhs;
        if (ptr)
            ptr->AddRef();
        return *this;
    }
    
    /// Assign another pointer.
    Ptr<T>& operator = (const Ptr<T>& rhs)
    {
        if (*this == rhs)
            return *this;

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
            ptr = nullptr;
        }
    }
    
    /// Perform a static cast from a pointer of another type.
    template <class U> void StaticCast(const Ptr<U>& rhs)
    {
        *this = static_cast<T*>(rhs.ptr);
    }

    /// Perform a dynamic cast from a weak pointer of another type.
    template <class U> void DynamicCast(const WeakPtr<U>& rhs)
    {
        Reset();
        T* rhsObject = dynamic_cast<T*>(rhs.ptr);
        if (rhsObject)
            *this = rhsObject;
    }
    
    /// Test for equality with another pointer.
    bool operator == (const Ptr<T>& rhs) const { return ptr == rhs.ptr; }
    /// Test for equality with a raw pointer.
    bool operator == (T* rhs) const { return ptr == rhs; }
    /// Test for inequality with another pointer.
    bool operator != (const Ptr<T>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a raw pointer.
    bool operator != (T* rhs) const { return !(*this == rhs); }
    /// Point to the object.
    T* operator -> () const { assert(ptr); return ptr; }
    /// Dereference the object.
    T& operator * () const { assert(ptr); return ptr; }
    /// Convert to the object.
    operator T* () const { return ptr; }
    
    /// Return the object.
    T* Get() const { return ptr; }
    /// Return the number of strong references.
    unsigned Refs() const { return ptr ? ptr->Refs() : 0; }
    /// Return the number of weak references.
    unsigned WeakRefs() const { return ptr ? ptr->WeakRefs() : 0; }
    /// Return whether is a null pointer.
    bool IsNull() const { return ptr == nullptr; }
    
private:
    /// %Object pointer.
    T* ptr;
};

/// Perform a static cast between pointers of two types.
template <class T, class U> Ptr<T> StaticCast(const Ptr<U>& rhs)
{
    Ptr<T> ret;
    ret.StaticCast(rhs);
    return ret;
}

/// Perform a dynamic cast between pointers of two types.
template <class T, class U> Ptr<T> DynamicCast(const Ptr<U>& rhs)
{
    Ptr<T> ret;
    ret.DynamicCast(rhs);
    return ret;
}

/// Pointer which holds a weak reference to a RefCounted subclass. Can track destruction but does not keep the object alive.
template <class T> class WeakPtr
{
public:
    /// Construct a null pointer.
    WeakPtr() :
        ptr(nullptr),
        refCount(nullptr)
    {
    }

    /// Copy-construct.
    WeakPtr(const WeakPtr<T>& ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        *this = ptr_;
    }

    /// Construct from a pointer.
    WeakPtr(const Ptr<T>& ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        *this = ptr_;
    }

    /// Construct from a raw pointer.
    WeakPtr(T* ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        *this = ptr_;
    }

    /// Destruct. Release the object reference.
    ~WeakPtr()
    {
        Reset();
    }

    /// Assign another weak pointer.
    WeakPtr<T>& operator = (const WeakPtr<T>& rhs)
    {
        if (*this == rhs)
            return *this;

        Reset();
        ptr = rhs.ptr;
        refCount = rhs.refCount;
        if (refCount)
            ++(refCount->weakRefs);
        return *this;
    }

    /// Assign a pointer.
    WeakPtr<T>& operator = (const Ptr<T>& rhs)
    {
        if (*this == rhs)
            return *this;

        Reset();
        ptr = rhs.Get();
        refCount = ptr ? ptr->RefCountPtr() : nullptr;
        if (refCount)
            ++(refCount->weakRefs);
        return *this;
    }

    /// Assign a raw pointer.
    WeakPtr<T>& operator = (T* rhs)
    {
        if (*this == rhs)
            return *this;

        Reset();
        ptr = rhs;
        refCount = ptr ? ptr->RefCountPtr() : nullptr;
        if (refCount)
            ++(refCount->weakRefs);
        return *this;
    }

    /// Release the weak object reference and reset to null.
    void Reset()
    {
        if (refCount)
        {
            --(refCount->weakRefs);
            // If expired and no more weak references, destroy the reference count
            if (refCount->expired && refCount->weakRefs == 0)
                delete refCount;
            ptr = nullptr;
            refCount = nullptr;
        }
    }

    /// Perform a static cast from a weak pointer of another type.
    template <class U> void StaticCast(const WeakPtr<U>& rhs)
    {
        *this = static_cast<T*>(rhs.ptr);
    }

    /// Perform a dynamic cast from a weak pointer of another type.
    template <class U> void DynamicCast(const WeakPtr<U>& rhs)
    {
        Reset();
        T* rhsObject = dynamic_cast<T*>(rhs.ptr);
        if (rhsObject)
            *this = rhsObject;
    }

    /// Test for equality with another weak pointer.
    bool operator == (const WeakPtr<T>& rhs) const { return ptr == rhs.ptr && refCount == rhs.refCount; }
    /// Test for equality with another pointer.
    bool operator == (const Ptr<T>& rhs) const { return ptr == rhs.Get(); }
    /// Test for equality with a raw pointer.
    bool operator == (T* rhs) const { return ptr == rhs; }
    /// Test for inequality with another weak pointer.
    bool operator != (const WeakPtr<T>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with another pointer.
    bool operator != (const Ptr<T>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a raw pointer.
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
        if (refCount && !refCount->expired)
            return ptr;
        else
            return nullptr;
    }

    /// Return the number of strong references.
    unsigned Refs() const { return refCount ? refCount->refs : 0; }
    /// Return the number of weak references.
    unsigned WeakRefs() const { return refCount ? refCount->weakRefs : 0; }
    /// Return whether is a null pointer.
    bool IsNull() const { return ptr == nullptr; }
    /// Return whether the object has been destroyed. Returns false if is a null pointer.
    bool IsExpired() const { return refCount && refCount->expired; }

private:
    /// %Object pointer.
    T* ptr;
    /// The object's weak reference count structure.
    RefCount* refCount;
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

/// Array pointer, which holds a strong reference to an array. Uses non-intrusive reference counting.
template <class T> class ArrayPtr
{
public:
    /// Construct a null pointer.
    ArrayPtr() :
        ptr(nullptr),
        refCount(nullptr)
    {
    }
    
    /// Copy-construct from another array pointer.
    ArrayPtr(const ArrayPtr<T>& ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        this = ptr_;
    }
    
    /// Construct from a raw pointer.
    explicit ArrayPtr(T* ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        this = ptr_;
    }
    
    /// Destruct. Release the array reference.
    ~ArrayPtr()
    {
        Reset();
    }
    
    /// Assign from another shared array pointer.
    ArrayPtr<T>& operator = (const ArrayPtr<T>& rhs)
    {
        if (*this == rhs)
            return *this;
        
        Reset();
        ptr = rhs.ptr;
        refCount = rhs.refCount;
        if (refCount)
            ++(refCount->refs);

        return *this;
    }
    
    /// Assign from a raw pointer.
    ArrayPtr<T>& operator = (T* rhs)
    {
        if (*this == rhs)
            return *this;
        
        Reset();

        if (ptr)
        {
            ptr = rhs;
            refCount = new RefCount();
            if (refCount)
                ++(refCount->refs);
        }
        
        return *this;
    }
    
    /// Point to the array.
    T* operator -> () const { assert(ptr); return ptr; }
    /// Dereference the array.
    T& operator * () const { assert(ptr); return *ptr; }
    /// Subscript the array.
    T& operator [] (const int index) { assert(ptr); return ptr[index]; }
    /// Test for equality with another array pointer.
    bool operator == (const ArrayPtr<T>& rhs) const { return ptr == rhs.ptr; }
    /// Test for inequality with another array pointer.
    bool operator != (const ArrayPtr<T>& rhs) const { return !(*this == rhs); }
    /// Convert to a raw pointer.
    operator T* () const { return ptr; }
    
    /// Release the array reference and reset to null. Destroy the array if was the last reference.
    void Reset()
    {
        if (refCount)
        {
            --(refCount->refs);
            if (refCount->refs == 0)
            {
                refCount->expired = true;
                delete[] ptr;
            }
            if (refCount->weakRefs == 0)
                delete refCount;
        }

        ptr = 0;
        refCount = 0;
    }
    
    /// Perform a static cast from an array pointer of another type.
    template <class U> void StaticCast(const ArrayPtr<U>& rhs)
    {
        Reset();
        ptr = static_cast<T*>(rhs.Get());
        refCount = rhs.RefCountPtr();
        if (refCount)
            ++(refCount->refs);
    }
    
   /// Perform a reinterpret cast from a shared array pointer of another type.
    template <class U> void ReinterpretCast(const ArrayPtr<U>& rhs)
    {
        Reset();
        ptr = reinterpret_cast<T*>(rhs.Get());
        refCount = rhs.RefCountPtr();
        if (refCount)
            ++(refCount->refs);
    }

    /// Return the raw pointer.
    T* Get() const { return ptr; }
    /// Check if the pointer is null.
    bool IsNull() const { return ptr == nullptr; }
    /// Return the number of strong references.
    unsigned Refs() const { return refCount ? refCount->refs : 0; }
    /// Return the number of weak references.
    unsigned WeakRefs() const { return refCount ? refCount->weakRefs : 0; }
    /// Return pointer to the reference count structure.
    RefCount* RefCountPtr() const { return refCount; }

private:
    /// Prevent direct assignment from an array pointer of different type.
    template <class U> ArrayPtr<T>& operator = (const ArrayPtr<U>& rhs);
    
    /// Pointer to the array.
    T* ptr;
    /// Pointer to the reference count structure.
    RefCount* refCount;
};

/// Perform a static cast from one shared array pointer type to another.
template <class T, class U> ArrayPtr<T> StaticCast(const ArrayPtr<U>& ptr)
{
    ArrayPtr<T> ret;
    ret.StaticCast(ptr);
    return ret;
}

/// Perform a reinterpret cast from one shared array pointer type to another.
template <class T, class U> ArrayPtr<T> ReinterpretCast(const ArrayPtr<U>& ptr)
{
    ArrayPtr<T> ret;
    ret.ReinterpretCast(ptr);
    return ret;
}

/// Weak array pointer. Can track destruction but does not keep the array alive. Uses non-intrusive reference counting.
template <class T> class WeakArrayPtr
{
public:
    /// Construct a null pointer.
    WeakArrayPtr() :
        ptr(nullptr),
        refCount(nullptr)
    {
    }
    
    /// Copy-construct from another weak array pointer.
    WeakArrayPtr(const WeakArrayPtr<T>& ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        *this = ptr_;
    }
    
    /// Construct from an array pointer.
    WeakArrayPtr(const ArrayPtr<T>& ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        *this = ptr_;
    }
    
    /// Destruct. Release the weak array reference.
    ~WeakArrayPtr()
    {
        Reset();
    }
    
    /// Assign from an array pointer.
    WeakArrayPtr<T>& operator = (const ArrayPtr<T>& rhs)
    {
        if (ptr == rhs.Get() && refCount == rhs.RefCountPtr())
            return *this;
        
        Reset();
        ptr = rhs.Get();
        refCount = rhs.RefCountPtr();
        if (refCount)
            ++(refCount->weakRefs);
        
        return *this;
    }
    
    /// Assign from another weak array pointer.
    WeakArrayPtr<T>& operator = (const WeakArrayPtr<T>& rhs)
    {
        if (ptr == rhs.ptr && refCount == rhs.refCount)
            return *this;
        
        Reset();
        ptr = rhs.ptr;
        refCount = rhs.refCount;
        if (refCount)
            ++(refCount->weakRefs);
        
        return *this;
    }

    /// Point to the array.
    T* operator -> () const
    {
        T* rawPtr = Get();
        assert(rawPtr);
        return rawPtr;
    }
    
    /// Dereference the array.
    T& operator * () const
    {
        T* rawPtr = Get();
        assert(rawPtr);
        return *rawPtr;
    }
    
    /// Subscript the array.
    T& operator [] (const int index)
    {
        T* rawPtr = Get();
        assert(rawPtr);
        return (*rawPtr)[index];
    }
    
    /// Test for equality with another weak array pointer.
    bool operator == (const WeakArrayPtr<T>& rhs) const { return ptr == rhs.ptr && refCount == rhs.refCount; }
    /// Test for inequality with another weak array pointer.
    bool operator != (const WeakArrayPtr<T>& rhs) const { return !(*this == rhs); }
    /// Convert to a raw pointer.
    operator T* () const { return Get(); }
    
    /// Release the weak array reference and reset to null.
    void Reset()
    {
        if (refCount)
        {
            --(refCount->weakRefs);
            if (refCount->expired && refCount->weakRefs == 0)
                delete refCount;
        }
        
        ptr = nullptr;
        refCount = nullptr;
    }
    
    /// Perform a static cast from a weak array pointer of another type.
    template <class U> void StaticCast(const WeakArrayPtr<U>& rhs)
    {
        Reset();
        ptr = static_cast<T*>(rhs.Get());
        refCount = rhs.refCount;
        if (refCount)
            ++(refCount->weakRefs);
    }
    
    /// Perform a reinterpret cast from a weak array pointer of another type.
    template <class U> void ReinterpretCast(const WeakArrayPtr<U>& rhs)
    {
        Reset();
        ptr = reinterpret_cast<T*>(rhs.Get());
        refCount = rhs.refCount;
        if (refCount)
            ++(refCount->weakRefs);
    }
    
    /// Return raw pointer. If array has destroyed, return null.
    T* Get() const
    {
        if (!refCount || refCount->expired)
            return nullptr;
        else
            return ptr;
    }

    /// Check if the pointer is null.
    bool IsNull() const { return refCount == nullptr; }
    /// Return number of strong references.
    unsigned Refs() const { return (refCount && refCount->refs >= 0) ? refCount->refs : 0; }
    /// Return number of weak references.
    unsigned WeakRefs() const { return refCount ? refCount->weakRefs : 0; }
    /// Return whether the array has been destroyed. Returns false if is a null pointer.
    bool IsExpired() const { return refCount ? refCount->expired : false; }
    /// Return pointer to the reference count structure.
    RefCount* RefCountPtr() const { return refCount; }

private:
    /// Prevent direct assignment from a weak array pointer of different type.
    template <class U> WeakArrayPtr<T>& operator = (const WeakArrayPtr<U>& rhs);

    /// Pointer to the array.
    T* ptr;
    /// Pointer to the RefCount structure.
    RefCount* refCount;
};

/// Perform a static cast from one weak array pointer type to another.
template <class T, class U> WeakArrayPtr<T> StaticCast(const WeakArrayPtr<U>& ptr)
{
    WeakArrayPtr<T> ret;
    ret.StaticCast(ptr);
    return ret;
}

/// Perform a reinterpret cast from one weak pointer type to another.
template <class T, class U> WeakArrayPtr<T> ReinterpretCast(const WeakArrayPtr<U>& ptr)
{
    WeakArrayPtr<T> ret;
    ret.ReinterpretCast(ptr);
    return ret;
}

}
