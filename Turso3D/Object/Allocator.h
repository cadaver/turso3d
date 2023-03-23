// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <cstddef>
#include <new>

struct AllocatorBlock;
struct AllocatorNode;

static const size_t DEFAULT_ALLOCATOR_INITIAL_CAPACITY = 16;

/// %Allocator memory block.
struct AllocatorBlock
{
    /// Size of a node.
    size_t nodeSize;
    /// Number of nodes in this block.
    size_t capacity;
    /// First free node.
    AllocatorNode* free;
    /// Next allocator block.
    AllocatorBlock* next;
    /// Nodes follow.
};

/// %Allocator node.
struct AllocatorNode
{
    /// Next free node.
    AllocatorNode* next;
    /// Data follows.
};

/// Initialize a fixed-size allocator with the node size and initial capacity.
AllocatorBlock* AllocatorInitialize(size_t nodeSize, size_t initialCapacity = DEFAULT_ALLOCATOR_INITIAL_CAPACITY);
/// Uninitialize a fixed-size allocator. Frees all blocks in the chain.
void AllocatorUninitialize(AllocatorBlock* allocator);
/// Allocate a node. Creates a new block if necessary.
void* AllocatorGet(AllocatorBlock* allocator);
/// Free a node. Does not free any blocks.
void AllocatorFree(AllocatorBlock* allocator, void* node);

/// %Allocator template class. Allocates objects of a specific class.
template <class T> class Allocator
{
public:
    /// Construct with initial capacity.
    Allocator(size_t capacity = DEFAULT_ALLOCATOR_INITIAL_CAPACITY) :
        allocator(nullptr)
    {
        if (capacity)
            Reserve(capacity);
    }
    
    /// Destruct. All objects reserved from this allocator should be freed before this is called.
    ~Allocator()
    {
        Reset();
    }
    
    /// Reserve initial capacity. Only possible before allocating the first object.
    void Reserve(size_t capacity)
    {
        if (!allocator)
            allocator = AllocatorInitialize(sizeof(T), capacity);
    }

    /// Allocate and default-construct an object.
    T* Allocate()
    {
        if (!allocator)
            allocator = AllocatorInitialize(sizeof(T));
        T* newObject = static_cast<T*>(AllocatorGet(allocator));
        new(newObject) T();
        
        return newObject;
    }
    
    /// Allocate and copy-construct an object.
    T* Allocate(const T& object)
    {
        if (!allocator)
            allocator = AllocatorInitialize(sizeof(T));
        T* newObject = static_cast<T*>(AllocatorGet(allocator));
        new(newObject) T(object);
        
        return newObject;
    }
    
    /// Destruct and free an object.
    void Free(T* object)
    {
        (object)->~T();
        AllocatorFree(allocator, object);
    }
    
    /// Free the allocator. All objects reserved from this allocator should be freed before this is called.
    void Reset()
    {
        AllocatorUninitialize(allocator);
        allocator = nullptr;
    }
    
private:
    /// Prevent copy construction.
    Allocator(const Allocator<T>& rhs);
    /// Prevent assignment.
    Allocator<T>& operator = (const Allocator<T>& rhs);
    
    /// Allocator block.
    AllocatorBlock* allocator;
};
