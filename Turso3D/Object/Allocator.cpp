// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "Allocator.h"

#include <cassert>

static AllocatorBlock* AllocatorGetBlock(AllocatorBlock* allocator, size_t nodeSize, size_t capacity)
{
    if (!capacity)
        capacity = 1;
    
    unsigned char* blockPtr = new unsigned char[sizeof(AllocatorBlock) + capacity * (sizeof(AllocatorNode) + nodeSize)];
    AllocatorBlock* newBlock = reinterpret_cast<AllocatorBlock*>(blockPtr);
    newBlock->nodeSize = nodeSize;
    newBlock->capacity = capacity;
    newBlock->free = nullptr;
    newBlock->next = nullptr;
    
    if (!allocator)
        allocator = newBlock;
    else
    {
        newBlock->next = allocator->next;
        allocator->next = newBlock;
    }
    
    // Initialize the nodes. Free nodes are always chained to the first (parent) allocator
    unsigned char* nodePtr = blockPtr + sizeof(AllocatorBlock);
    AllocatorNode* firstNewNode = reinterpret_cast<AllocatorNode*>(nodePtr);
    
    for (size_t i = 0; i < capacity - 1; ++i)
    {
        AllocatorNode* newNode = reinterpret_cast<AllocatorNode*>(nodePtr);
        newNode->next = reinterpret_cast<AllocatorNode*>(nodePtr + sizeof(AllocatorNode) + nodeSize);
        nodePtr += sizeof(AllocatorNode) + nodeSize;
    }
    // i == capacity - 1
    {
        AllocatorNode* newNode = reinterpret_cast<AllocatorNode*>(nodePtr);
        newNode->next = nullptr;
    }
    
    allocator->free = firstNewNode;
    return newBlock;
}

AllocatorBlock* AllocatorInitialize(size_t nodeSize, size_t initialCapacity)
{
    AllocatorBlock* block = AllocatorGetBlock(nullptr, nodeSize, initialCapacity);
    return block;
}

void AllocatorUninitialize(AllocatorBlock* allocator)
{
    while (allocator)
    {
        AllocatorBlock* next = allocator->next;
        delete[] reinterpret_cast<unsigned char*>(allocator);
        allocator = next;
    }
}

void* AllocatorGet(AllocatorBlock* allocator)
{
    if (!allocator)
        return nullptr;
    
    if (!allocator->free)
    {
        // Free nodes have been exhausted. Allocate a new larger block
        size_t newCapacity = (allocator->capacity + 1) >> 1;
        AllocatorGetBlock(allocator, allocator->nodeSize, newCapacity);
        allocator->capacity += newCapacity;
    }
    
    // We should have new free node(s) chained
    AllocatorNode* freeNode = allocator->free;
    void* ptr = (reinterpret_cast<unsigned char*>(freeNode)) + sizeof(AllocatorNode);
    allocator->free = freeNode->next;
    freeNode->next = nullptr;
    
    return ptr;
}

void AllocatorFree(AllocatorBlock* allocator, void* ptr)
{
    if (!allocator || !ptr)
        return;
    
    unsigned char* dataPtr = static_cast<unsigned char*>(ptr);
    AllocatorNode* node = reinterpret_cast<AllocatorNode*>(dataPtr - sizeof(AllocatorNode));
    
    assert(!node->next);
    if (node->next)
        LOGERROR("Potential illegal free of object not allocated via the allocator");

    // Chain the node back to free nodes
    node->next = allocator->free;
    allocator->free = node;
}
