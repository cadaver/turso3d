// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

#include <cstddef>
#include <new>

namespace Turso3D
{

struct AllocatorBlock;
struct AllocatorNode;

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
TURSO3D_API AllocatorBlock* AllocatorInitialize(size_t nodeSize, size_t initialCapacity = 1);
/// Uninitialize a fixed-size allocator. Frees all blocks in the chain.
TURSO3D_API void AllocatorUninitialize(AllocatorBlock* allocator);
/// Reserve a node. Creates a new block if necessary.
TURSO3D_API void* AllocatorReserve(AllocatorBlock* allocator);
/// Free a node. Does not free any blocks.
TURSO3D_API void AllocatorFree(AllocatorBlock* allocator, void* node);

/// %Allocator template class. Allocates objects of a specific class.
template <class T> class Allocator
{
public:
    /// Construct.
    Allocator(size_t initialCapacity = 0) :
        allocator(0)
    {
        if (initialCapacity)
            allocator = AllocatorInitialize(sizeof(T), initialCapacity);
    }
    
    /// Destruct. All objects reserved from this allocator should be freed before this is called.
    ~Allocator()
    {
        AllocatorUninitialize(allocator);
    }
    
    /// Reserve and default-construct an object.
    T* Reserve()
    {
        if (!allocator)
            allocator = AllocatorInitialize(sizeof(T));
        T* newObject = static_cast<T*>(AllocatorReserve(allocator));
        new(newObject) T();
        
        return newObject;
    }
    
    /// Reserve and copy-construct an object.
    T* Reserve(const T& object)
    {
        if (!allocator)
            allocator = AllocatorInitialize(sizeof(T));
        T* newObject = static_cast<T*>(AllocatorReserve(allocator));
        new(newObject) T(object);
        
        return newObject;
    }
    
    /// Destruct and free an object.
    void Free(T* object)
    {
        (object)->~T();
        AllocatorFree(allocator, object);
    }
    
private:
    /// Prevent copy construction.
    Allocator(const Allocator<T>& rhs);
    /// Prevent assignment.
    Allocator<T>& operator = (const Allocator<T>& rhs);
    
    /// Allocator block.
    AllocatorBlock* allocator;
};

}
