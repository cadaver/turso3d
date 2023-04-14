// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <cassert>
#include <cstddef>

class RefCounted;
template <class T> class WeakPtr;

/// Reference count structure. Used in both intrusive and non-intrusive reference counting.
struct RefCount
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

/// Base class for intrusively reference counted objects that can be pointed to with SharedPtr and WeakPtr. These are not copy-constructible and not assignable.
class RefCounted
{
    friend class Object;

public:
    /// Construct. The reference count is not allocated yet; it will be allocated on demand.
    RefCounted();

    /// Destruct. If no weak references, destroy also the reference count, else mark it expired.
    virtual ~RefCounted();

    /// Add a strong reference. Allocate the reference count structure first if necessary.
    void AddRef();
    /// Release a strong reference. Destroy the object when the last strong reference is gone.
    virtual void ReleaseRef();

    /// Return the number of strong references.
    unsigned Refs() const { return refCount ? refCount->refs : 0; }
    /// Return the number of weak references.
    unsigned WeakRefs() const { return refCount ? refCount->weakRefs : 0; }
    /// Return pointer to the reference count structure. Allocate if not allocated yet.
    RefCount* RefCountPtr();

    /// Allocate a reference count structure.
    static RefCount* AllocateRefCount();
    /// Free a reference count structure.
    static void FreeRefCount(RefCount* refCount);

private:
    /// Prevent copy construction.
    RefCounted(const RefCounted& rhs);
    /// Prevent assignment.
    RefCounted& operator = (const RefCounted& rhs);

    /// Reference count structure, allocated on demand.
    RefCount* refCount;
};

/// Pointer which holds a strong reference to a RefCounted subclass and allows shared ownership.
template <class T> class SharedPtr
{
public:
    /// Construct a null pointer.
    SharedPtr() :
        ptr(nullptr)
    {
    }
    
    /// Copy-construct.
    SharedPtr(const SharedPtr<T>& ptr_) :
        ptr(nullptr)
    {
        *this = ptr_;
    }

    /// Construct from a raw pointer.
    SharedPtr(T* ptr_) :
        ptr(nullptr)
    {
        *this = ptr_;
    }
    
    /// Destruct. Release the object reference and destroy the object if was the last strong reference.
    ~SharedPtr()
    {
        Reset();
    }
    
    /// Assign a raw pointer.
    SharedPtr<T>& operator = (T* rhs)
    {
        if (Get() == rhs)
            return *this;

        Reset();
        ptr = rhs;
        if (ptr)
            reinterpret_cast<RefCounted*>(ptr)->AddRef();
        return *this;
    }
    
    /// Assign another shared pointer.
    SharedPtr<T>& operator = (const SharedPtr<T>& rhs)
    {
        if (*this == rhs)
            return *this;

        Reset();
        ptr = rhs.ptr;
        if (ptr)
            reinterpret_cast<RefCounted*>(ptr)->AddRef();
        return *this;
    }
    
    /// Release the object reference and reset to null. Destroy the object if was the last reference.
    void Reset()
    {
        if (ptr)
        {
            reinterpret_cast<RefCounted*>(ptr)->ReleaseRef();
            ptr = nullptr;
        }
    }
    
    /// Test for equality with another shared pointer.
    bool operator == (const SharedPtr<T>& rhs) const { return ptr == rhs.ptr; }
    /// Test for equality with a raw pointer.
    bool operator == (T* rhs) const { return ptr == rhs; }
    /// Test for inequality with another shared pointer.
    bool operator != (const SharedPtr<T>& rhs) const { return !(*this == rhs); }
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

    /// Construct from a shared pointer.
    WeakPtr(const SharedPtr<T>& ptr_) :
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

    /// Assign a shared pointer.
    WeakPtr<T>& operator = (const SharedPtr<T>& rhs)
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
        if (Get() == rhs)
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
                RefCounted::FreeRefCount(refCount);
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
    /// Test for equality with a shared pointer.
    bool operator == (const SharedPtr<T>& rhs) const { return ptr == rhs.Get(); }
    /// Test for equality with a raw pointer.
    bool operator == (T* rhs) const { return ptr == rhs; }
    /// Test for inequality with another weak pointer.
    bool operator != (const WeakPtr<T>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a shared pointer.
    bool operator != (const SharedPtr<T>& rhs) const { return !(*this == rhs); }
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

/// Pointer which holds a strong reference to an array and allows shared ownership. Uses non-intrusive reference counting.
template <class T> class SharedArrayPtr
{
public:
    /// Construct a null pointer.
    SharedArrayPtr() :
        ptr(nullptr),
        refCount(nullptr)
    {
    }
    
    /// Copy-construct from another shared array pointer.
    SharedArrayPtr(const SharedArrayPtr<T>& ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        *this = ptr_;
    }
    
    /// Construct from a raw pointer. To avoid double refcount and double delete, create only once from the same raw pointer.
    explicit SharedArrayPtr(T* ptr_) :
        ptr(nullptr),
        refCount(nullptr)
    {
        *this = ptr_;
    }
    
    /// Destruct. Release the array reference.
    ~SharedArrayPtr()
    {
        Reset();
    }
    
    /// Assign from another shared array pointer.
    SharedArrayPtr<T>& operator = (const SharedArrayPtr<T>& rhs)
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
    
    /// Assign from a raw pointer. To avoid double refcount and double delete, assign only once from the same raw pointer.
    SharedArrayPtr<T>& operator = (T* rhs)
    {
        if (Get() == rhs)
            return *this;
        
        Reset();

        if (rhs)
        {
            ptr = rhs;
            refCount = RefCounted::AllocateRefCount();
            if (refCount)
                ++(refCount->refs);
        }
        
        return *this;
    }
    
    /// Test for equality with another shared array pointer.
    bool operator == (const SharedArrayPtr<T>& rhs) const { return ptr == rhs.ptr; }
    /// Test for inequality with another shared array pointer.
    bool operator != (const SharedArrayPtr<T>& rhs) const { return !(*this == rhs); }
    /// Point to the array.
    T* operator -> () const { assert(ptr); return ptr; }
    /// Dereference the array.
    T& operator * () const { assert(ptr); return *ptr; }
    /// Convert to the array element pointer.
    operator T* () const { return Get(); }

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
                // If no weak refs, destroy the ref count now too
                if (refCount->weakRefs == 0)
                    RefCounted::FreeRefCount(refCount);
            }
        }

        ptr = 0;
        refCount = 0;
    }
    
    /// Perform a static cast from a shared array pointer of another type.
    template <class U> void StaticCast(const SharedArrayPtr<U>& rhs)
    {
        Reset();
        ptr = static_cast<T*>(rhs.Get());
        refCount = rhs.RefCountPtr();
        if (refCount)
            ++(refCount->refs);
    }
    
   /// Perform a reinterpret cast from a shared array pointer of another type.
    template <class U> void ReinterpretCast(const SharedArrayPtr<U>& rhs)
    {
        Reset();
        ptr = reinterpret_cast<T*>(rhs.Get());
        refCount = rhs.RefCountPtr();
        if (refCount)
            ++(refCount->refs);
    }

    /// Return the raw pointer.
    T* Get() const { return ptr; }
    /// Return the number of strong references.
    unsigned Refs() const { return refCount ? refCount->refs : 0; }
    /// Return the number of weak references.
    unsigned WeakRefs() const { return refCount ? refCount->weakRefs : 0; }
    /// Return pointer to the reference count structure.
    RefCount* RefCountPtr() const { return refCount; }
    /// Check if the pointer is null.
    bool IsNull() const { return ptr == nullptr; }

private:
    /// Prevent direct assignment from an array pointer of different type.
    template <class U> SharedArrayPtr<T>& operator = (const SharedArrayPtr<U>& rhs);
    
    /// Pointer to the array.
    T* ptr;
    /// Pointer to the reference count structure.
    RefCount* refCount;
};

/// Perform a static cast from one shared array pointer type to another.
template <class T, class U> SharedArrayPtr<T> StaticCast(const SharedArrayPtr<U>& ptr)
{
    SharedArrayPtr<T> ret;
    ret.StaticCast(ptr);
    return ret;
}

/// Perform a reinterpret cast from one shared array pointer type to another.
template <class T, class U> SharedArrayPtr<T> ReinterpretCast(const SharedArrayPtr<U>& ptr)
{
    SharedArrayPtr<T> ret;
    ret.ReinterpretCast(ptr);
    return ret;
}

/// Pointer which holds a weak reference to an array. Can track destruction but does not keep the object alive. Uses non-intrusive reference counting.
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
    
    /// Construct from a shared array pointer.
    WeakArrayPtr(const SharedArrayPtr<T>& ptr_) :
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
    
    /// Assign from a shared array pointer.
    WeakArrayPtr<T>& operator = (const SharedArrayPtr<T>& rhs)
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
    
    /// Index the array.
    T& operator [] (size_t index)
    {
        T* rawPtr = Get();
        assert(rawPtr);
        return (*rawPtr)[index];
    }

    /// Const-index the array.
    const T& operator [] (size_t index) const
    {
        T* rawPtr = Get();
        assert(rawPtr);
        return (*rawPtr)[index];
    }
    
    /// Test for equality with another weak array pointer.
    bool operator == (const WeakArrayPtr<T>& rhs) const { return ptr == rhs.ptr && refCount == rhs.refCount; }
    /// Test for equality with a shared array pointer.
    bool operator == (const SharedArrayPtr<T>& rhs) const { return ptr == rhs.ptr && refCount == rhs.refCount; }
    /// Test for inequality with another weak array pointer.
    bool operator != (const WeakArrayPtr<T>& rhs) const { return !(*this == rhs); }
    /// Test for inequality with a shared array pointer.
    bool operator != (const SharedArrayPtr<T>& rhs) const { return !(*this == rhs); }
    /// Convert to bool.
    operator bool() const { return Get() != nullptr; }

    /// Release the weak array reference and reset to null.
    void Reset()
    {
        if (refCount)
        {
            --(refCount->weakRefs);
            if (refCount->expired && refCount->weakRefs == 0)
                RefCounted::FreeRefCount(refCount);
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
