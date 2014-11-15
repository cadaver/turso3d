// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Iterator.h"
#include "Swap.h"

#include <cassert>
#include <cstring>
#include <new>

#ifdef _MSC_VER
#pragma warning(disable:4345)
#endif

namespace Turso3D
{

/// %Vector base class.
class TURSO3D_API VectorBase
{
public:
    /// Construct.
    VectorBase();

    /// Swap with another vector.
    void Swap(VectorBase& vector);

    /// Return number of elements in the vector.
    size_t Size() const { return buffer ? reinterpret_cast<size_t*>(buffer)[0] : 0; }
    /// Return element capacity of the vector.
    size_t Capacity() const { return buffer ? reinterpret_cast<size_t*>(buffer)[1] : 0; }
    /// Return whether has no elements.
    bool IsEmpty() const { return Size() == 0; }

protected:
    /// Set new size.
    void SetSize(size_t size) { reinterpret_cast<size_t*>(buffer)[0] = size; } 
    /// Set new capacity.
    void SetCapacity(size_t capacity) { reinterpret_cast<size_t*>(buffer)[1] = capacity; } 

    /// Allocate the buffer for elements.
    static unsigned char* AllocateBuffer(size_t size);

    /// Buffer. Contains size and capacity in the beginning.
    unsigned char* buffer;
};

/// %Vector template class. Implements a dynamic-sized array where the elements are in continuous memory. The elements need to be safe to move with block copy.
template <class T> class Vector : public VectorBase
{
public:
    typedef RandomAccessIterator<T> Iterator;
    typedef RandomAccessConstIterator<T> ConstIterator;

    /// Construct empty.
    Vector()
    {
    }

    /// Construct with initial size.
    explicit Vector(size_t startSize)
    {
        Resize(startSize, 0);
    }

    /// Construct with initial data.
    Vector(const T* data, size_t dataSize)
    {
        Resize(dataSize, data);
    }

    /// Copy-construct.
    Vector(const Vector<T>& vector)
    {
        *this = vector;
    }

    /// Destruct.
    ~Vector()
    {
        Clear();
        delete[] buffer;
    }

    /// Assign from another vector.
    Vector<T>& operator = (const Vector<T>& rhs)
    {
        Clear();
        Resize(rhs.Size(), rhs.Buffer());
        return *this;
    }

    /// Add-assign an element.
    Vector<T>& operator += (const T& rhs)
    {
        Push(rhs);
        return *this;
    }

    /// Add-assign another vector.
    Vector<T>& operator += (const Vector<T>& rhs)
    {
        Push(rhs);
        return *this;
    }

    /// Add an element.
    Vector<T> operator + (const T& rhs) const
    {
        Vector<T> ret(*this);
        ret.Push(rhs);
        return ret;
    }

    /// Add another vector.
    Vector<T> operator + (const Vector<T>& rhs) const
    {
        Vector<T> ret(*this);
        ret.Push(rhs);
        return ret;
    }

    /// Test for equality with another vector.
    bool operator == (const Vector<T>& rhs) const
    {
        size_t size = Size();
        if (rhs.Size() != size)
            return false;

        T* buffer = Buffer();
        T* rhsBuffer = rhs.Buffer();

        for (size_t i = 0; i < size; ++i)
        {
            if (buffer[i] != rhsBuffer[i])
                return false;
        }

        return true;
    }

    /// Test for inequality with another vector.
    bool operator != (const Vector<T>& rhs) const { return !(*this == rhs); }
    /// Return element at index.
    T& operator [] (size_t index) { assert(index < Size()); return Buffer()[index]; }
    /// Return const element at index.
    const T& operator [] (size_t index) const { assert(index < Size()); return Buffer()[index]; }

    /// Add an element at the end.
    void Push(const T& value) { Resize(Size() + 1, &value); }
    /// Add another vector at the end.
    void Push(const Vector<T>& vector) { Resize(Size() + vector.Size(), vector.Buffer()); }

    /// Remove the last element.
    void Pop()
    {
        if (Size())
            Resize(Size() - 1, 0);
    }

    /// Insert an element at position.
    void Insert(size_t pos, const T& value)
    {
        if (pos > Size())
            pos = Size();

        size_t oldSize = Size();
        Resize(Size() + 1, 0);
        MoveRange(pos + 1, pos, oldSize - pos);
        Buffer()[pos] = value;
    }

    /// Insert another vector at position.
    void Insert(size_t pos, const Vector<T>& vector)
    {
        if (pos > Size())
            pos = Size();

        size_t oldSize = Size();
        Resize(Size() + vector.Size(), 0);
        MoveRange(pos + vector.Size(), pos, oldSize - pos);
        CopyElements(Buffer() + pos, vector.Buffer(), vector.Size());
    }

    /// Insert an element by iterator.
    Iterator Insert(const Iterator& dest, const T& value)
    {
        size_t pos = dest - Begin();
        if (pos > Size())
            pos = Size();
        Insert(pos, value);

        return Begin() + pos;
    }

    /// Insert a vector by iterator.
    Iterator Insert(const Iterator& dest, const Vector<T>& vector)
    {
        size_t pos = dest - Begin();
        if (pos > Size())
            pos = Size();
        Insert(pos, vector);

        return Begin() + pos;
    }

    /// Insert a vector partially by iterators.
    Iterator Insert(const Iterator& dest, const ConstIterator& start, const ConstIterator& end)
    {
        size_t pos = dest - Begin();
        if (pos > Size())
            pos = Size();
        size_t length = end - start;
        Resize(Size() + length, 0);
        MoveRange(pos + length, pos, Size() - pos - length);

        T* destPtr = Buffer() + pos;
        for (Iterator it = start; it != end; ++it)
            *destPtr++ = *it;

        return Begin() + pos;
    }

    /// Insert elements.
    Iterator Insert(const Iterator& dest, const T* start, const T* end)
    {
        size_t pos = dest - Begin();
        if (pos > Size())
            pos = Size();
        size_t length = end - start;
        Resize(Size() + length, 0);
        MoveRange(pos + length, pos, Size() - pos - length);

        T* destPtr = Buffer() + pos;
        for (const T* i = start; i != end; ++i)
            *destPtr++ = *i;

        return Begin() + pos;
    }

    /// Erase a range of elements.
    void Erase(size_t pos, size_t length = 1)
    {
        // Return if the range is illegal
        if (pos + length > Size() || !length)
            return;

        MoveRange(pos, pos + length, Size() - pos - length);
        Resize(Size() - length, 0);
    }

    /// Erase an element by iterator. Return iterator to the next element.
    Iterator Erase(const Iterator& it)
    {
        size_t pos = it - Begin();
        if (pos >= Size())
            return End();
        Erase(pos);

        return Begin() + pos;
    }

    /// Erase a range by iterators. Return iterator to the next element.
    Iterator Erase(const Iterator& start, const Iterator& end)
    {
        size_t pos = start - Begin();
        if (pos >= Size())
            return End();
        size_t length = end - start;
        Erase(pos, length);

        return Begin() + pos;
    }

    /// Erase an element if found.
    bool Remove(const T& value)
    {
        Iterator it = Find(value);
        if (it != End())
        {
            Erase(it);
            return true;
        }
        else
            return false;
    }

    /// Clear the vector.
    void Clear() { Resize(0); }
    /// Resize the vector.
    void Resize(size_t newSize) { Resize(newSize, 0); }

    /// Set new capacity.
    void Reserve(size_t newCapacity)
    {
        size_t size = Size();
        if (newCapacity < size)
            newCapacity = size;

        if (newCapacity != Capacity())
        {
            unsigned char* newBuffer = nullptr;
            
            if (newCapacity)
            {
                newBuffer = AllocateBuffer(newCapacity * sizeof(T));
                // Move the data into the new buffer
                // This assumes the elements are safe to move without copy-constructing and deleting the old elements;
                // ie. they should not contain pointers to self, or interact with outside objects in their constructors
                // or destructors
                MoveElements(reinterpret_cast<T*>(newBuffer + 2 * sizeof(size_t)), Buffer(), size);
            }

            // Delete the old buffer
            delete[] buffer;
            buffer = newBuffer;
            SetSize(size);
            SetCapacity(newCapacity);
        }
    }

    /// Reallocate so that no extra memory is used.
    void Compact() { Reserve(Size()); }

    /// Return element at index.
    T& At(size_t index) { assert(index < Size()); return Buffer()[index]; }
    /// Return const element at index.
    const T& At(size_t index) const { assert(index < Size()); return Buffer()[index]; }

    /// Return iterator to first occurrence of value, or to the end if not found.
    Iterator Find(const T& value)
    {
        Iterator it = Begin();
        while (it != End() && *it != value)
            ++it;
        return it;
    }

    /// Return const iterator to first occurrence of value, or to the end if not found.
    ConstIterator Find(const T& value) const
    {
        ConstIterator it = Begin();
        while (it != End() && *it != value)
            ++it;
        return it;
    }

    /// Return whether contains a specific value.
    bool Contains(const T& value) const { return Find(value) != End(); }
    /// Return iterator to the beginning.
    Iterator Begin() { return Iterator(Buffer()); }
    /// Return const iterator to the beginning.
    ConstIterator Begin() const { return ConstIterator(Buffer()); }
    /// Return iterator to the end.
    Iterator End() { return Iterator(Buffer() + Size()); }
    /// Return const iterator to the end.
    ConstIterator End() const { return ConstIterator(Buffer() + Size()); }
    /// Return first element.
    T& Front() { assert(Size()); return Buffer()[0]; }
    /// Return const first element.
    const T& Front() const { assert(Size()); return Buffer()[0]; }
    /// Return last element.
    T& Back() { assert(Size()); return Buffer()[Size() - 1]; }
    /// Return const last element.
    const T& Back() const { assert(Size()); return Buffer()[Size() - 1]; }

private:
    /// Return the buffer with right type.
    T* Buffer() const { return reinterpret_cast<T*>(buffer + 2 * sizeof(size_t)); }

   /// Resize the vector and create/remove new elements as necessary.
    void Resize(size_t newSize, const T* src)
    {
        size_t size = Size();
        size_t capacity = Capacity();

        // If size shrinks, destruct the removed elements
        if (newSize < size)
            DestructElements(Buffer() + newSize, size - newSize);
        else
        {
            // Allocate new buffer if necessary and copy the current elements
            if (newSize > capacity)
            {
                if (!capacity)
                    capacity = newSize;
                else
                {
                    while (capacity < newSize)
                        capacity += (capacity + 1) >> 1;
                }

                unsigned char* newBuffer = AllocateBuffer(capacity * sizeof(T));
                if (buffer)
                {
                    MoveElements(reinterpret_cast<T*>(newBuffer + 2 * sizeof(size_t)), Buffer(), size);
                    delete[] buffer;
                }
                buffer = newBuffer;
                SetCapacity(capacity);
            }

            // Initialize the new elements. Optimize for case of only 1 element
            size_t count = newSize - size;
            T* dest = Buffer() + size;
            if (src)
            {
                if (count == 1)
                    new(dest) T(*src);
                else
                {
                    T* end = dest + count;
                    while (dest != end)
                    {
                        new(dest) T(*src);
                        ++dest;
                        ++src;
                    }
                }
            }
            else
            {
                if (count == 1)
                    new(dest) T();
                else
                {
                    T* end = dest + count;
                    while (dest != end)
                    {
                        new(dest) T();
                        ++dest;
                    }
                }
            }
        }

        if (buffer)
            SetSize(newSize);
    }

    /// Move a range of elements within the vector.
    void MoveRange(size_t dest, size_t src, size_t count)
    {
        T* buffer = Buffer();
        if (src < dest)
        {
            for (size_t i = count - 1; i < count; --i)
                buffer[dest + i] = buffer[src + i];
        }
        if (src > dest)
        {
            for (size_t i = 0; i < count; ++i)
                buffer[dest + i] = buffer[src + i];
        }
    }

    /// Copy elements from one buffer to another.
    static void CopyElements(T* dest, const T* src, size_t count)
    {
        while (count--)
            *dest++ = *src++;
    }

    /// Move elements from one buffer to another. Constructors or destructors are not called.
    static void MoveElements(T* dest, const T* src, size_t count)
    {
        memcpy(dest, src, sizeof(T) * count);
    }

    // Call the elements' destructors.
    static void DestructElements(T* dest, size_t count)
    {
        while (count--)
        {
            dest->~T();
            ++dest;
        }
    }
};

}
