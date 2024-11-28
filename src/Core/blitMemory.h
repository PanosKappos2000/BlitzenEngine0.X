#pragma once

#include "blitAssert.h"

namespace BlitzenCore
{
    enum class AllocationType : uint8_t
    {
        Unkown = 0, 
        Array = 1, 
        DynamicArray = 2,
        Hashmap = 3,
        Queue = 4, 
        Bst = 5, 
        Engine = 7, 
        Renderer = 8, 
        Entity = 9,
        EntityNode = 10,
        Scene = 11,

        MaxTypes = 12
    };

    struct AllocationData
    {
        size_t totalAllocated;
        size_t typesAllocated[static_cast<size_t>(AllocationType::MaxTypes)];
    };

    void MemoryManagementInit();
    void MemoryManagementShutdown();

    void* BlitAlloc(AllocationType alloc, size_t size);
    void BlitFree(AllocationType alloc, void* pBlock, size_t size);
    void* BlitMemoryCopy(void* pDst, void* pSrc, size_t size);
    void* BlitMemorySet(void* pDst, int32_t value, size_t size);
    void* BlitMemoryZero(void* pDst, size_t size);
}