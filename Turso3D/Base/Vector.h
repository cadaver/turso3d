// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Iterator.h"
#include "Swap.h"

#include <cassert>
#include <cstring>
#include <new>

namespace Turso3D
{

/// %Vector base class.
class VectorBase
{
public:
    /// Construct.
    VectorBase() :
        size(0),
        capacity(0),
        buffer(0)
    {
    }

    /// Swap with another vector.
    void Swap(VectorBase& rhs)
    {
        Turso3D::Swap(size, rhs.size);
        Turso3D::Swap(capacity, rhs.capacity);
        Turso3D::Swap(buffer, rhs.buffer);
    }

    /// Return number of elements in the vector.
    size_t Size() const { return size; }
    /// Return element capacity of the vector.
    size_t Capacity() const { return capacity; }
    /// Return whether the vector is empty.
    bool IsEmpty() const { return size == 0; }

protected:
    /// Allocate the buffer for elements.
    static unsigned char* AllocateBuffer(size_t size);

    /// Number of elements.
    size_t size;
    /// Element capacity in the buffer.
    size_t capacity;
    /// Buffer.
    unsigned char* buffer;
};

/// %Vector template class. Implements a dynamic-sized array where the elements are in continuous memory.
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
        Resize(rhs.size, rhs.Buffer());
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

    /// Add an element at the end.
    void Push(const T& value) { Resize(size + 1, &value); }
    /// Add another vector at the end.
    void Push(const Vector<T>& vector) { Resize(size + vector.size, vector.Buffer()); }

    /// Remove the last element.
    void Pop()
    {
        if (size)
            Resize(size - 1, 0);
    }

    /// Insert an element at position.
    void Insert(size_t pos, const T& value)
    {
        if (pos > size)
            pos = size;

        size_t oldSize = size;
        Resize(size + 1, 0);
        MoveRange(pos + 1, pos, oldSize - pos);
        Buffer()[pos] = value;
    }

    /// Insert another vector at position.
    void Insert(size_t pos, const Vector<T>& vector)
    {
        if (pos > size)
            pos = size;

        size_t oldSize = size;
        Resize(size + vector.size, 0);
        MoveRange(pos + vector.size, pos, oldSize - pos);
        CopyElements(Buffer() + pos, vector.Buffer(), vector.size);
    }

    /// Insert an element by iterator.
    Iterator Insert(const Iterator& dest, const T& value)
    {
        size_t pos = dest - Begin();
        if (pos > size)
            pos = size;
        Insert(pos, value);

        return Begin() + pos;
    }

    /// Insert a vector by iterator.
    Iterator Insert(const Iterator& dest, const Vector<T>& vector)
    {
        size_t pos = dest - Begin();
        if (pos > size)
            pos = size;
        Insert(pos, vector);

        return Begin() + pos;
    }

    /// Insert a vector partially by iterators.
    Iterator Insert(const Iterator& dest, const ConstIterator& start, const ConstIterator& end)
    {
        size_t pos = dest - Begin();
        if (pos > size)
            pos = size;
        size_t length = end - start;
        Resize(size + length, 0);
        MoveRange(pos + length, pos, size - pos - length);

        T* destPtr = Buffer() + pos;
        for (Iterator it = start; it != end; ++it)
            *destPtr++ = *it;

        return Begin() + pos;
    }

    /// Insert elements.
    Iterator Insert(const Iterator& dest, const T* start, const T* end)
    {
        size_t pos = dest - Begin();
        if (pos > size)
            pos = size;
        size_t length = end - start;
        Resize(size + length, 0);
        MoveRange(pos + length, pos, size - pos - length);

        T* destPtr = Buffer() + pos;
        for (const T* i = start; i != end; ++i)
            *destPtr++ = *i;

        return Begin() + pos;
    }

    /// Erase a range of elements.
    void Erase(size_t pos, size_t length = 1)
    {
        // Return if the range is illegal
        if (pos + length > size || !length)
            return;

        MoveRange(pos, pos + length, size - pos - length);
        Resize(size - length, 0);
    }

    /// Erase an element by iterator. Return iterator to the next element.
    Iterator Erase(const Iterator& it)
    {
        size_t pos = it - Begin();
        if (pos >= size)
            return End();
        Erase(pos);

        return Begin() + pos;
    }

    /// Erase a range by iterators. Return iterator to the next element.
    Iterator Erase(const Iterator& start, const Iterator& end)
    {
        size_t pos = start - Begin();
        if (pos >= size)
            return End();
        size_t length = end - start;
        Erase(pos, length);

        return Begin() + pos;
    }

    /// Erase an element if found.
    bool Remove(const T& value)
    {
        Iterator i = Find(value);
        if (i != End())
        {
            Erase(i);
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
        if (newCapacity < size)
            newCapacity = size;

        if (newCapacity != capacity)
        {
            T* newBuffer = 0;
            capacity = newCapacity;

            if (capacity)
            {
                newBuffer = reinterpret_cast<T*>(AllocateBuffer(capacity * sizeof(T)));
                // Move the data into the new buffer
                ConstructElements(newBuffer, Buffer(), size);
            }

            // Delete the old buffer
            DestructElements(Buffer(), size);
            delete[] buffer;
            buffer = reinterpret_cast<unsigned char*>(newBuffer);
        }
    }

    /// Reallocate so that no extra memory is used.
    void Compact() { Reserve(size); }

    /// Test for equality with another vector.
    bool operator == (const Vector<T>& rhs) const
    {
        if (rhs.size != size)
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
    T& operator [] (size_t index) { assert(index < size); return Buffer()[index]; }
    /// Return const element at index.
    const T& operator [] (size_t index) const { assert(index < size); return Buffer()[index]; }

    /// Return element at index.
    T& At(size_t index) { assert(index < size); return Buffer()[index]; }
    /// Return const element at index.
    const T& At(size_t index) const { assert(index < size); return Buffer()[index]; }

    /// Return iterator to value, or to the end if not found.
    Iterator Find(const T& value)
    {
        Iterator it = Begin();
        while (it != End() && *it != value)
            ++it;
        return it;
    }

    /// Return const iterator to value, or to the end if not found.
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
    Iterator End() { return Iterator(Buffer() + size); }
    /// Return const iterator to the end.
    ConstIterator End() const { return ConstIterator(Buffer() + size); }
    /// Return first element.
    T& Front() { assert(size); return Buffer()[0]; }
    /// Return const first element.
    const T& Front() const { assert(size); return Buffer()[0]; }
    /// Return last element.
    T& Back() { assert(size); return Buffer()[size - 1]; }
    /// Return const last element.
    const T& Back() const { assert(size); return Buffer()[size - 1]; }

private:
    /// Return the buffer with right type.
    T* Buffer() const { return reinterpret_cast<T*>(buffer); }

   /// Resize the vector and create/remove new elements as necessary.
    void Resize(size_t newSize, const T* src)
    {
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
                    ConstructElements(reinterpret_cast<T*>(newBuffer), Buffer(), size);
                    DestructElements(Buffer(), size);
                    delete[] buffer;
                }
                buffer = newBuffer;
            }

            // Initialize the new elements
            ConstructElements(Buffer() + size, src, newSize - size);
        }

        size = newSize;
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

    /// Construct elements, optionally with source data.
    static void ConstructElements(T* dest, const T* src, size_t count)
    {
        if (!src)
        {
            for (size_t i = 0; i < count; ++i)
                new(dest + i) T();
        }
        else
        {
            for (size_t i = 0; i < count; ++i)
                new(dest + i) T(*(src + i));
        }
    }

    /// Copy elements from one buffer to another.
    static void CopyElements(T* dest, const T* src, size_t count)
    {
        while (count--)
            *dest++ = *src++;
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

/// %Vector template class for POD types. Does not call constructors or destructors and uses block move.
template <class T> class PODVector : public VectorBase
{
public:
    typedef RandomAccessIterator<T> Iterator;
    typedef RandomAccessConstIterator<T> ConstIterator;

    /// Construct empty.
    PODVector()
    {
    }

    /// Construct with initial size.
    explicit PODVector(size_t startSize)
    {
        Resize(startSize);
    }

    /// Construct with initial data.
    PODVector(const T* data, size_t dataSize)
    {
        Resize(dataSize);
        CopyElements(Buffer(), data, dataSize);
    }

    /// Copy-construct.
    PODVector(const PODVector<T>& vector)
    {
        *this = vector;
    }

    /// Destruct.
    ~PODVector()
    {
        delete[] buffer;
    }

    /// Assign from another vector.
    PODVector<T>& operator = (const PODVector<T>& rhs)
    {
        Resize(rhs.size);
        CopyElements(Buffer(), rhs.Buffer(), rhs.size);
        return *this;
    }

    /// Add-assign an element.
    PODVector<T>& operator += (const T& rhs)
    {
        Push(rhs);
        return *this;
    }

    /// Add-assign another vector.
    PODVector<T>& operator += (const PODVector<T>& rhs)
    {
        Push(rhs);
        return *this;
    }

    /// Add an element.
    PODVector<T> operator + (const T& rhs) const
    {
        PODVector<T> ret(*this);
        ret.Push(rhs);
        return ret;
    }

    /// Add another vector.
    PODVector<T> operator + (const PODVector<T>& rhs) const
    {
        PODVector<T> ret(*this);
        ret.Push(rhs);
        return ret;
    }

    /// Add an element at the end.
    void Push(const T& value)
    {
        if (size < capacity)
            ++size;
        else
            Resize(size + 1);
        Back() = value;
    }

    /// Add another vector at the end.
    void Push(const PODVector<T>& vector)
    {
        size_t oldSize = size;
        Resize(size + vector.size);
        CopyElements(Buffer() + oldSize, vector.Buffer(), vector.size_);
    }

    /// Remove the last element.
    void Pop()
    {
        if (size)
            Resize(size - 1);
    }

    /// Insert an element at position.
    void Insert(size_t pos, const T& value)
    {
        if (pos > size)
            pos = size;

        size_t oldSize = size;
        Resize(size + 1);
        MoveRange(pos + 1, pos, oldSize - pos);
        Buffer()[pos] = value;
    }

    /// Insert another vector at position.
    void Insert(size_t pos, const PODVector<T>& vector)
    {
        if (pos > size)
            pos = size;

        size_t oldSize = size;
        Resize(size + vector.size);
        MoveRange(pos + vector.size, pos, oldSize - pos);
        CopyElements(Buffer() + pos, vector.Buffer(), vector.size_);
    }

    /// Insert an element by iterator.
    Iterator Insert(const Iterator& dest, const T& value)
    {
        size_t pos = dest - Begin();
        if (pos > size)
            pos = size;
        Insert(pos, value);

        return Begin() + pos;
    }

    /// Insert a vector by iterator.
    Iterator Insert(const Iterator& dest, const PODVector<T>& vector)
    {
        size_t pos = dest - Begin();
        if (pos > size)
            pos = size;
        Insert(pos, vector);

        return Begin() + pos;
    }

    /// Insert a vector partially by iterators.
    Iterator Insert(const Iterator& dest, const ConstIterator& start, const ConstIterator& end)
    {
        size_t pos = dest - Begin();
        if (pos > size)
            pos = size;
        size_t length = end - start;
        Resize(size + length);
        MoveRange(pos + length, pos, size - pos - length);
        CopyElements(Buffer() + pos, &(*start), length);

        return Begin() + pos;
    }

    /// Insert elements.
    Iterator Insert(const Iterator& dest, const T* start, const T* end)
    {
        size_t pos = dest - Begin();
        if (pos > size)
            pos = size;
        size_t length = end - start;
        Resize(size + length);
        MoveRange(pos + length, pos, size - pos - length);

        T* destPtr = Buffer() + pos;
        for (const T* i = start; i != end; ++i)
            *destPtr++ = *i;

        return Begin() + pos;
    }

    /// Erase a range of elements.
    void Erase(size_t pos, size_t length = 1)
    {
        // Return if the range is illegal
        if (!length || pos + length > size)
            return;

        MoveRange(pos, pos + length, size - pos - length);
        Resize(size - length);
    }

    /// Erase an element by iterator. Return iterator to the next element.
    Iterator Erase(const Iterator& it)
    {
        size_t pos = it - Begin();
        if (pos >= size)
            return End();
        Erase(pos);

        return Begin() + pos;
    }

    /// Erase a range by iterators. Return iterator to the next element.
    Iterator Erase(const Iterator& start, const Iterator& end)
    {
        size_t pos = start - Begin();
        if (pos >= size)
            return End();
        size_t length = end - start;
        Erase(pos, length);

        return Begin() + pos;
    }

    /// Erase an element if found.
    bool Remove(const T& value)
    {
        Iterator i = Find(value);
        if (i != End())
        {
            Erase(i);
            return true;
        }
        else
            return false;
    }

    /// Clear the vector.
    void Clear() { Resize(0); }

    /// Resize the vector.
    void Resize(size_t newSize)
    {
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
            // Move the data into the new buffer and delete the old
            if (buffer)
            {
                CopyElements(reinterpret_cast<T*>(newBuffer), Buffer(), size);
                delete[] buffer;
            }
            buffer = newBuffer;
        }

        size = newSize;
    }

    /// Set new capacity.
    void Reserve(size_t newCapacity)
    {
        if (newCapacity < size)
            newCapacity = size;

        if (newCapacity != capacity)
        {
            unsigned char* newBuffer = 0;
            capacity = newCapacity;

            if (capacity)
            {
                newBuffer = AllocateBuffer(capacity * sizeof(T));
                // Move the data into the new buffer
                CopyElements(reinterpret_cast<T*>(newBuffer), Buffer(), size);
            }

            // Delete the old buffer
            delete[] buffer;
            buffer = newBuffer;
        }
    }

    /// Reallocate so that no extra memory is used.
    void Compact() { Reserve(size); }

    /// Test for equality with another vector.
    bool operator == (const PODVector<T>& rhs) const
    {
        if (rhs.size != size)
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
    bool operator != (const PODVector<T>& rhs) const { return !(*this == rhs); }
    /// Return element at index.
    T& operator [] (size_t index) { assert(index < size); return Buffer()[index]; }
    /// Return const element at index.
    const T& operator [] (size_t index) const { assert(index < size); return Buffer()[index]; }

    /// Return element at index.
    T& At(size_t index) { assert(index < size); return Buffer()[index]; }
    /// Return const element at index.
    const T& At(size_t index) const { assert(index < size); return Buffer()[index]; }

    /// Return iterator to value, or to the end if not found.
    Iterator Find(const T& value)
    {
        Iterator it = Begin();
        while (it != End() && *it != value)
            ++it;
        return it;
    }

    /// Return const iterator to value, or to the end if not found.
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
    Iterator End() { return Iterator(Buffer() + size); }
    /// Return const iterator to the end.
    ConstIterator End() const { return ConstIterator(Buffer() + size); }
    /// Return first element.
    T& Front() { return Buffer()[0]; }
    /// Return const first element.
    const T& Front() const { return Buffer()[0]; }
    /// Return last element.
    T& Back() { assert(size); return Buffer()[size - 1]; }
    /// Return const last element.
    const T& Back() const { assert(size); return Buffer()[size - 1]; }

private:
    /// Return the buffer with right type.
    T* Buffer() const { return reinterpret_cast<T*>(buffer); }

    /// Move a range of elements within the vector.
    void MoveRange(size_t dest, size_t src, size_t count)
    {
        if (count)
            memmove(Buffer() + dest, Buffer() + src, count * sizeof(T));
    }

    /// Copy elements from one buffer to another.
    static void CopyElements(T* dest, const T* src, size_t count)
    {
        if (count)
            memcpy(dest, src, count * sizeof(T));
    }
};

}
