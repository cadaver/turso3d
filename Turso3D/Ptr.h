// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

/// Pointer which takes ownership of an object and deletes it when the pointer goes out of scope. Ownership can be transferred to another pointer, in which case the source pointer becomes null.
template <class T> class AutoPtr
{
public:
    /// Construct a null pointer.
    AutoPtr() :
        ptr(0)
    {
    }

    /// Construct and take ownership of the object
    AutoPtr(T* object) :
        ptr(object)
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
    AutoPtr<T>& operator = (T* object)
    {
        delete ptr;
        ptr = object;
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

    /// Return the object pointed to.
    T* Get() const { return ptr; }
    
private:
    /// Object pointer.
    T* ptr;
};