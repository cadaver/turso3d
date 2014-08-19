// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Allocator.h"
#include "Hash.h"
#include "Sort.h"
#include "Vector.h"

namespace Turso3D
{

/// Hash set template class.
template <class T> class HashSet : public HashBase
{
public:
    /// Hash set node.
    struct Node : public HashNodeBase
    {
        /// Construct undefined.
        Node()
        {
        }
        
        /// Construct with key.
        Node(const T& rhs) :
            key(rhs)
        {
        }
        
        /// Key.
        T key;
        
        /// Return next node.
        Node* Next() const { return static_cast<Node*>(next); }
        /// Return previous node.
        Node* Prev() const { return static_cast<Node*>(prev); }
        /// Return next node in the bucket.
        Node* Down() const { return static_cast<Node*>(down); }
    };
    
    /// Hash set node iterator.
    struct Iterator : public HashIteratorBase
    {
        /// Construct.
        Iterator()
        {
        }
        
        /// Construct with a node pointer.
        Iterator(Node* rhs) :
            HashIteratorBase(rhs)
        {
        }
        
        /// Preincrement the pointer.
        Iterator& operator ++ () { GotoNext(); return *this; }
        /// Postincrement the pointer.
        Iterator operator ++ (int) { Iterator it = *this; GotoNext(); return it; }
        /// Predecrement the pointer.
        Iterator& operator -- () { GotoPrev(); return *this; }
        /// Postdecrement the pointer.
        Iterator operator -- (int) { Iterator it = *this; GotoPrev(); return it; }
        
        /// Point to the key.
        const T* operator -> () const { return &(static_cast<Node*>(ptr))->key; }
        /// Dereference the key.
        const T& operator * () const { return (static_cast<Node*>(ptr))->key; }
    };
    
    /// Hash set node const iterator.
    struct ConstIterator : public HashIteratorBase
    {
        /// Construct.
        ConstIterator()
        {
        }
        
        /// Construct with a node pointer.
        ConstIterator(Node* rhs) :
            HashIteratorBase(rhs)
        {
        }
        
        /// Construct from a non-const iterator.
        ConstIterator(const Iterator& rhs) :
            HashIteratorBase(rhs.ptr)
        {
        }
        
        /// Assign from a non-const iterator.
        ConstIterator& operator = (const Iterator& rhs) { ptr = rhs.ptr; return *this; }
        /// Preincrement the pointer.
        ConstIterator& operator ++ () { GotoNext(); return *this; }
        /// Postincrement the pointer.
        ConstIterator operator ++ (int) { ConstIterator it = *this; GotoNext(); return it; }
        /// Predecrement the pointer.
        ConstIterator& operator -- () { GotoPrev(); return *this; }
        /// Postdecrement the pointer.
        ConstIterator operator -- (int) { ConstIterator it = *this; GotoPrev(); return it; }
        
        /// Point to the key.
        const T* operator -> () const { return &(static_cast<Node*>(ptr))->key; }
        /// Dereference the key.
        const T& operator * () const { return (static_cast<Node*>(ptr))->key; }
    };
    
    /// Construct empty.
    HashSet()
    {
        // Reserve the tail node
        allocator = AllocatorInitialize(sizeof(Node));
        head = tail = AllocateNode();
    }
    
    /// Construct from another hash set.
    HashSet(const HashSet<T>& set)
    {
        // Reserve the tail node + initial capacity according to the set's size
        allocator = AllocatorInitialize(sizeof(Node), set.Size() + 1);
        head = tail = AllocateNode();
        *this = set;
    }
    
    /// Destruct.
    ~HashSet()
    {
        Clear();
        FreeNode(Tail());
        AllocatorUninitialize(allocator);
    }
    
    /// Assign a hash set.
    HashSet& operator = (const HashSet<T>& rhs)
    {
        Clear();
        Insert(rhs);
        return *this;
    }
    
    /// Add-assign a value.
    HashSet& operator += (const T& rhs)
    {
        Insert(rhs);
        return *this;
    }
    
    /// Add-assign a hash set.
    HashSet& operator += (const HashSet<T>& rhs)
    {
        Insert(rhs);
        return *this;
    }

    /// Test for equality with another hash set.
    bool operator == (const HashSet<T>& rhs) const
    {
        if (rhs.Size() != Size())
            return false;
        
        for (ConstIterator it = Begin(); it != End(); ++it)
        {
            if (!rhs.Contains(*it))
                return false;
        }
        
        return true;
    }
    
    /// Test for inequality with another hash set.
    bool operator != (const HashSet<T>& rhs) const { return !(*this == rhs); }

    /// Insert a key. Return an iterator to it.
    Iterator Insert(const T& key)
    {
        // If no pointers yet, allocate with minimum bucket count
        if (!ptrs)
        {
            AllocateBuckets(Size(), MIN_BUCKETS);
            Rehash();
        }
        
        unsigned hashKey = Hash(key);
        
        Node* existing = FindNode(key, hashKey);
        if (existing)
            return Iterator(existing);
        
        Node* newNode = InsertNode(Tail(), key);
        newNode->down = Ptrs()[hashKey];
        Ptrs()[hashKey] = newNode;
        
        // Rehash if the maximum load factor has been exceeded
        if (Size() > NumBuckets() * MAX_LOAD_FACTOR)
        {
            AllocateBuckets(Size(), NumBuckets() << 1);
            Rehash();
        }
        
        return Iterator(newNode);
    }
    
    /// Insert a set.
    void Insert(const HashSet<T>& set)
    {
        for (ConstIterator it = set.Begin(); it != set.End(); ++it)
            Insert(*it);
    }
    
    /// Insert a key by iterator. Return iterator to the value.
    Iterator Insert(const ConstIterator& it)
    {
        return Iterator(InsertNode(*it));
    }
    
    /// Erase a key. Return true if was found.
    bool Erase(const T& key)
    {
        if (!ptrs)
            return false;
        
        unsigned hashKey = Hash(key);
        
        Node* previous;
        Node* node = FindNode(key, hashKey, previous);
        if (!node)
            return false;
        
        if (previous)
            previous->down = node->down;
        else
            Ptrs()[hashKey] = node->down;
        
        EraseNode(node);
        return true;
    }
    
    /// Erase a key by iterator. Return iterator to the next key.
    Iterator Erase(const Iterator& it)
    {
        if (!ptrs || !it.ptr)
            return End();
        
        Node* node = static_cast<Node*>(it.ptr);
        Node* next = node->Next();
        
        unsigned hashKey = Hash(node->key);
        
        Node* previous = 0;
        Node* current = static_cast<Node*>(Ptrs()[hashKey]);
        while (current && current != node)
        {
            previous = current;
            current = current->Down();
        }
        
        assert(current == node);
        
        if (previous)
            previous->down = node->down;
        else
            Ptrs()[hashKey] = node->down;
        
        EraseNode(node);
        return Iterator(next);
    }
    
    /// Clear the set.
    void Clear()
    {
        if (Size())
        {
            for (Iterator it = Begin(); it != End(); )
            {
                FreeNode(static_cast<Node*>(it++.ptr));
                it.ptr->prev = 0;
            }
            
            head = tail;
            SetSize(0);
        }
        
        ResetPtrs();
    }
    
    /// Sort keys. After sorting the set can be iterated in order until new elements are inserted.
    void Sort()
    {
        size_t numKeys = Size();
        if (!numKeys)
            return;
        
        Node** ptrs = new Node*[numKeys];
        Node* ptr = Head();
        
        for (size_t i = 0; i < numKeys; ++i)
        {
            ptrs[i] = ptr;
            ptr = ptr->Next();
        }
        
        Turso3D::Sort(RandomAccessIterator<Node*>(ptrs), RandomAccessIterator<Node*>(ptrs + numKeys), CompareNodes);
        
        head = ptrs[0];
        ptrs[0]->prev = 0;
        for (size_t i = 1; i < numKeys; ++i)
        {
            ptrs[i - 1]->next = ptrs[i];
            ptrs[i]->prev = ptrs[i - 1];
        }
        ptrs[numKeys - 1]->next = tail;
        tail->prev = ptrs[numKeys - 1];
        
        delete[] ptrs;
    }
    
    /// Rehash to a specific bucket count, which must be a power of two. Return true on success.
    bool Rehash(size_t numBuckets)
    {
        if (numBuckets == NumBuckets())
            return true;
        if (!numBuckets || numBuckets < Size() / MAX_LOAD_FACTOR)
            return false;
        
        // Check for being power of two
        size_t check = numBuckets;
        while (!(check & 1))
            check >>= 1;
        if (check != 1)
            return false;
        
        AllocateBuckets(Size(), numBuckets);
        Rehash();
        return true;
    }
    
    /// Return iterator to the key, or end iterator if not found.
    Iterator Find(const T& key)
    {
        if (!ptrs)
            return End();
        
        unsigned hashKey = Hash(key);
        Node* node = FindNode(key, hashKey);
        if (node)
            return Iterator(node);
        else
            return End();
    }
    
    /// Return const iterator to the key, or end iterator if not found.
    ConstIterator Find(const T& key) const
    {
        if (!ptrs)
            return End();
        
        unsigned hashKey = Hash(key);
        Node* node = FindNode(key, hashKey);
        if (node)
            return ConstIterator(node);
        else
            return End();
    }
    
    /// Return whether contains a key.
    bool Contains(const T& key) const
    {
        if (!ptrs)
            return false;
        
        unsigned hashKey = Hash(key);
        return FindNode(key, hashKey) != 0;
    }
    
    /// Return iterator to the first element. Is not the lowest value unless the set has been sorted.
    Iterator Begin() { return Iterator(Head()); }
    /// Return const iterator to the first element. Is not the lowest value unless the set has been sorted.
    ConstIterator Begin() const { return ConstIterator(Head()); }
    /// Return iterator to the end.
    Iterator End() { return Iterator(Tail()); }
    /// Return const iterator to the end.
    ConstIterator End() const { return ConstIterator(Tail()); }
    /// Return first key. Is not the lowest value unless the set has been sorted.
    const T& Front() const { return *Begin(); }
    /// Return last key.
    const T& Back() const { assert(Size()); return *(--End()); }
    
private:
    /// Return the head node.
    Node* Head() const { return static_cast<Node*>(head); }
    /// Return the tail node.
    Node* Tail() const { return static_cast<Node*>(tail); }
    
    /// Find a node from the buckets. Do not call if the buckets have not been allocated.
    Node* FindNode(const T& key, unsigned hashKey) const
    {
        Node* node = static_cast<Node*>(Ptrs()[hashKey]);
        while (node)
        {
            if (node->key == key)
                return node;
            node = node->Down();
        }
        
        return 0;
    }
    
    /// Find a node and the previous node from the buckets. Do not call if the buckets have not been allocated.
    Node* FindNode(const T& key, unsigned hashKey, Node*& previous) const
    {
        previous = 0;
        
        Node* node = static_cast<Node*>(Ptrs()[hashKey]);
        while (node)
        {
            if (node->key == key)
                return node;
            previous = node;
            node = node->Down();
        }
        
        return 0;
    }
    
    /// Insert a node into the list. Return the new node.
    Node* InsertNode(Node* dest, const T& key)
    {
        if (!dest)
            return 0;
        
        Node* newNode = AllocateNode(key);
        Node* prev = dest->Prev();
        newNode->next = dest;
        newNode->prev = prev;
        if (prev)
            prev->next = newNode;
        dest->prev = newNode;
        
        // Reassign the head node if necessary
        if (dest == Head())
            head = newNode;
        
        SetSize(Size() + 1);
        
        return newNode;
    }
    
    /// Erase a node from the list. Return pointer to the next element, or to the end if could not erase.
    Node* EraseNode(Node* node)
    {
        // The tail node can not be removed
        if (!node || node == tail)
            return Tail();
        
        Node* prev = node->Prev();
        Node* next = node->Next();
        if (prev)
            prev->next = next;
        next->prev = prev;
        
        // Reassign the head node if necessary
        if (node == Head())
            head = next;
        
        FreeNode(node);
        SetSize(Size() - 1);
        
        return next;
    }
    
    /// Allocate a node with optionally specified key.
    Node* AllocateNode(const T& key = T())
    {
        Node* newNode = static_cast<Node*>(AllocatorGet(allocator));
        new(newNode) Node(key);
        return newNode;
    }
    
    /// Free a node.
    void FreeNode(Node* node)
    {
        (node)->~Node();
        AllocatorFree(allocator, node);
    }
    
    /// Rehash the buckets.
    void Rehash()
    {
        for (Iterator it = Begin(); it != End(); ++it)
        {
            Node* node = static_cast<Node*>(it.ptr);
            unsigned hashKey = Hash(*it);
            node->down = Ptrs()[hashKey];
            Ptrs()[hashKey] = node;
        }
    }
    
    /// Compare two nodes.
    static bool CompareNodes(Node*& lhs, Node*& rhs) { return lhs->key < rhs->key; }

    /// Compute a hash based on the key and the bucket size
    unsigned Hash(const T& key) const { return MakeHash(key) & (NumBuckets() - 1); }
};

}
