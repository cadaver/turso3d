// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

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

    /// Copy-construct. Ownership is transferred, making the source pointer null.
    AutoPtr(const AutoPtr<T>& ptr_) :
        ptr(ptr_.ptr)
    {
        // Trick the compiler so that the AutoPtr can be copied to containers; the latest copy stays non-null
        const_cast<AutoPtr<T>&>(ptr_).ptr = 0;
    }

    /// Construct with a raw pointer; take ownership of the object.
    AutoPtr(T* ptr_) :
       ptr(ptr_)
    {
    }

    /// Destruct. Delete the object pointed to.
    ~AutoPtr()
    {
        delete ptr;
    }

    /// Assign from a pointer. Existing object is deleted and ownership is transferred from the source pointer, which becomes null.
    AutoPtr<T>& operator = (const AutoPtr<T>& rhs)
    {
        delete ptr;
        ptr = rhs.ptr;
        const_cast<AutoPtr<T>&>(rhs).ptr = 0;
        return *this;
    }

    /// Assign a new object. Existing object is deleted.
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

/// Pointer which takes ownership of an array allocated with new[] and deletes it when the pointer goes out of scope.
template <class T> class AutoArrayPtr
{
public:
    /// Construct a null pointer.
    AutoArrayPtr() :
        array(0)
    {
    }

    /// Copy-construct. Ownership is transferred, making the source pointer null.
    AutoArrayPtr(AutoArrayPtr<T>& ptr) :
        array(ptr.array)
    {
        ptr.array = 0;
    }
    
    /// Construct and take ownership of the array.
    AutoArrayPtr(T* array_) :
       array(array_)
    {
    }

    /// Destruct. Delete the array pointed to.
    ~AutoArrayPtr()
    {
        delete[] array;
    }

    /// Assign from a pointer. Existing array is deleted and ownership is transferred from the source pointer, which becomes null.
    AutoArrayPtr<T>& operator = (AutoArrayPtr<T>& rhs)
    {
        delete array;
        array = rhs.array;
        rhs.array = 0;
        return *this;
    }

    /// Assign a new array. Existing array is deleted.
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

}
