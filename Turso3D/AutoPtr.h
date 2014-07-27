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

/// Pointer which takes ownership of an array allocated with new[] and deletes it when the pointer goes out of scope.
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

}
