// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Allocator.h"
#include "Swap.h"

namespace Turso3D
{

/// Doubly-linked list node base class.
struct TURSO3D_API ListNodeBase
{
    /// Construct.
    ListNodeBase() :
        prev(0),
        next(0)
    {
    }
    
    /// Previous node.
    ListNodeBase* prev;
    /// Next node.
    ListNodeBase* next;
};

/// Doubly-linked list iterator base class.
struct TURSO3D_API ListIteratorBase
{
    /// Construct.
    ListIteratorBase() :
        ptr(0)
    {
    }
    
    /// Construct with a node pointer.
    explicit ListIteratorBase(ListNodeBase* ptr_) :
        ptr(ptr_)
    {
    }
    
    /// Test for equality with another iterator.
    bool operator == (const ListIteratorBase& rhs) const { return ptr == rhs.ptr; }
    /// Test for inequality with another iterator.
    bool operator != (const ListIteratorBase& rhs) const { return ptr != rhs.ptr; }
    
    /// Go to the next node.
    void GotoNext()
    {
        if (ptr)
            ptr = ptr->next;
    }
    
    /// Go to the previous node.
    void GotoPrev()
    {
        if (ptr)
            ptr = ptr->prev;
    }
    
    /// Node pointer.
    ListNodeBase* ptr;
};

/// Doubly-linked list base class.
class TURSO3D_API ListBase
{
public:
    /// Construct.
    ListBase() :
        allocator(0),
        size(0)
    {
    }
    
    /// Swap with another linked list.
    void Swap(ListBase& list)
    {
        Turso3D::Swap(head, list.head);
        Turso3D::Swap(tail, list.tail);
        Turso3D::Swap(allocator, list.allocator);
        Turso3D::Swap(size, list.size);
    }

    /// Return number of elements.
    size_t Size() const { return size; }
    /// Return whether has no elements.
    bool IsEmpty() const { return size == 0; }
    
protected:
    /// Head node pointer.
    ListNodeBase* head;
    /// Tail node pointer.
    ListNodeBase* tail;
    /// Node allocator.
    AllocatorBlock* allocator;
    /// Number of elements.
    size_t size;
};

/// Doubly-linked list template class. Elements can generally be assumed to be in non-continuous memory.
template <class T> class List : public ListBase
{
public:
    /// %List node.
    struct Node : public ListNodeBase
    {
        /// Construct undefined.
        Node()
        {
        }
        
        /// Construct with value.
        Node(const T& value_) :
            value(value_)
        {
        }
        
        /// Node value.
        T value;
        
        /// Return next node.
        Node* Next() const { return static_cast<Node*>(next); }
        /// Return previous node.
        Node* Prev() { return static_cast<Node*>(prev); }
    };
    
    /// %List iterator.
    struct Iterator : public ListIteratorBase
    {
        /// Construct.
        Iterator()
        {
        }
        
        /// Construct with a node pointer.
        explicit Iterator(Node* ptr_) :
            ListIteratorBase(ptr_)
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
        
        /// Point to the node value.
        T* operator -> () const { return &(static_cast<Node*>(ptr))->value; }
        /// Dereference the node value.
        T& operator * () const { return (static_cast<Node*>(ptr))->value; }
    };
    
    /// %List const iterator.
    struct ConstIterator : public ListIteratorBase
    {
        /// Construct.
        ConstIterator()
        {
        }
        
        /// Construct with a node pointer.
        explicit ConstIterator(Node* ptr_) :
            ListIteratorBase(ptr_)
        {
        }
        
        /// Construct from a non-const iterator.
        ConstIterator(const Iterator& it) :
            ListIteratorBase(it.ptr)
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
        
        /// Point to the node value.
        const T* operator -> () const { return &(static_cast<Node*>(ptr))->value; }
        /// Dereference the node value.
        const T& operator * () const { return (static_cast<Node*>(ptr))->value; }
    };

    /// Construct empty.
    List()
    {
        allocator = AllocatorInitialize(sizeof(Node));
        head = tail = AllocateNode();
    }
    
    /// Copy-construct.
    List(const List<T>& list)
    {
        // Reserve the tail node + initial capacity according to the list's size
        allocator = AllocatorInitialize(sizeof(Node), list.Size() + 1);
        head = tail = AllocateNode();
        *this = list;
    }

    /// Destruct.
    ~List()
    {
        Clear();
        FreeNode(Tail());
        AllocatorUninitialize(allocator);
    }
    
    /// Assign from another list.
    List& operator = (const List<T>& rhs)
    {
        // Clear, then insert the nodes of the other list
        Clear();
        Insert(End(), rhs);
        return *this;
    }
    
    /// Add-assign an element.
    List& operator += (const T& rhs)
    {
        Push(rhs);
        return *this;
    }
    
    /// Add-assign a list.
    List& operator += (const List<T>& rhs)
    {
        Insert(End(), rhs);
        return *this;
    }
    
    /// Test for equality with another list.
    bool operator == (const List<T>& rhs) const
    {
        if (rhs.size != size)
            return false;
        
        ConstIterator i = Begin();
        ConstIterator j = rhs.Begin();
        while (i != End())
        {
            if (*i != *j)
                return false;
            ++i;
            ++j;
        }
        
        return true;
    }
    
    /// Test for inequality with another list.
    bool operator != (const List<T>& rhs) const { return !(*this == rhs); }
    
    /// Insert an element to the end.
    void Push(const T& value) { InsertNode(Tail(), value); }
    /// Insert an element to the beginning.
    void PushFront(const T& value) { InsertNode(Head(), value); }
    /// Insert an element at position.
    void Insert(const Iterator& dest, const T& value) { InsertNode(static_cast<Node*>(dest.ptr_), value); }
    
    /// Insert a list at position.
    void Insert(const Iterator& dest, const List<T>& list)
    {
        Node* destNode = static_cast<Node*>(dest.ptr);
        ConstIterator it = list.Begin();
        ConstIterator end = list.End();
        while (it != end)
            InsertNode(destNode, *it++);
    }
    
    /// Insert elements by iterators.
    void Insert(const Iterator& dest, const ConstIterator& start, const ConstIterator& end)
    {
        Node* destNode = static_cast<Node*>(dest.ptr);
        ConstIterator it = start;
        while (it != end)
            InsertNode(destNode, *it++);
    }
    
    /// Insert elements.
    void Insert(const Iterator& dest, const T* start, const T* end)
    {
        Node* destNode = static_cast<Node*>(dest.ptr);
        const T* ptr = start;
        while (ptr != end)
            InsertNode(destNode, *ptr++);
    }
    
    /// Erase the last element.
    void Pop()
    {
        if (size)
            Erase(--End());
    }
    
    /// Erase the first element.
    void PopFront()
    {
        if (size)
            Erase(Begin());
    }
    
    /// Erase an element by iterator. Return iterator to the next element.
    Iterator Erase(Iterator it)
    {
        return Iterator(EraseNode(static_cast<Node*>(it.ptr)));
    }
    
    /// Erase a range by iterators. Return an iterator to the next element.
    Iterator Erase(const Iterator& start, const Iterator& end)
    {
        Iterator it = start;
        while (it != end)
            it = EraseNode(static_cast<Node*>(it.ptr));
        
        return it;
    }
    
    /// Clear the list.
    void Clear()
    {
        if (Size())
        {
            for (Iterator i = Begin(); i != End(); )
            {
                FreeNode(static_cast<Node*>(i++.ptr));
                i.ptr->prev = 0;
            }
            
            head = tail;
            size = 0;
        }
    }
    
    /// Resize the list by removing or adding items at the end.
    void Resize(size_t newSize)
    {
        while (size > newSize)
            Pop();
        
        while (size < newSize)
            InsertNode(Tail(), T());
    }
    
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
    /// Return iterator to the first element.
    Iterator Begin() { return Iterator(Head()); }
    /// Return const iterator to the first element.
    ConstIterator Begin() const { return ConstIterator(Head()); }
    /// Return iterator to the end.
    Iterator End() { return Iterator(Tail()); }
    /// Return const iterator to the end.
    ConstIterator End() const { return ConstIterator(Tail()); }
    /// Return first element.
    T& Front() { return *Begin(); }
    /// Return const first element.
    const T& Front() const { return *Begin(); }
    /// Return last element.
    T& Back() { return *(--End()); }
    /// Return const last element.
    const T& Back() const { return *(--End()); }
    
private:
    /// Return the head node.
    Node* Head() const { return static_cast<Node*>(head); }
    /// Return the tail node.
    Node* Tail() const { return static_cast<Node*>(tail); }
    
    /// Allocate and insert a node into the list.
    void InsertNode(Node* dest, const T& value)
    {
        if (!dest)
            return;
        
        Node* newNode = AllocateNode(value);
        Node* prev = dest->Prev();
        newNode->next = dest;
        newNode->prev = prev;
        if (prev)
            prev->next = newNode;
        dest->prev = newNode;
        
        // Reassign the head node if necessary
        if (dest == Head())
            head = newNode;
        
        ++size;
    }
    
    /// Erase and free a node. Return pointer to the next node, or to the end if could not erase.
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
        --size;
        
        return next;
    }
    
    /// Reserve a node with optionally specified value.
    Node* AllocateNode(const T& = T())
    {
        Node* newNode = static_cast<Node*>(AllocatorGet(allocator));
        new(newNode) Node();
        return newNode;
    }
    
    /// Free a node.
    void FreeNode(Node* node)
    {
        (node)->~Node();
        AllocatorFree(allocator, node);
    }
};

}

