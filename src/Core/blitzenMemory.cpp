#include "blitMemory.h"
#include "Platform/blitPlatform.h"
#include "mainEngine.h"

namespace BlitzenCore
{
    static AllocationData allocState;

    void MemoryManagementInit()
    {
        BlitzenPlatform::PlatformMemZero(&allocState, sizeof(AllocationData));
    }

    void MemoryManagementShutdown()
    {
        if (BlitzenEngine::Engine::GetEngineInstancePointer())
        {
            BLIT_ERROR("Blitzen is still active, memory management cannot be shutdown")
            return;
        }
        BLIT_ASSERT_MESSAGE(!allocState.totalAllocated, "There is still unallocated memory")
    }

    void* BlitAlloc(AllocationType alloc, size_t size)
    {
        // This might need to be an assertion so that the application fails, when memory is mihandled
        if(alloc == AllocationType::Unkown || alloc == AllocationType::MaxTypes)
        {
            BLIT_FATAL("Allocation type: %i, A valid allocation type must be specified!", static_cast<uint8_t>(alloc))
        }

        allocState.totalAllocated += size;
        allocState.typesAllocated[static_cast<size_t>(alloc)] += size;

        return BlitzenPlatform::PlatformMalloc(size, false);
    }

    void BlitFree(AllocationType alloc, void* pBlock, size_t size)
    {
        // This might need to be an assertion so that the application fails, when memory is mihandled
        if(alloc == AllocationType::Unkown || alloc == AllocationType::MaxTypes)
        {
            BLIT_FATAL("Allocation type: %i, A valid allocation type must be specified!", static_cast<uint8_t>(alloc))
        }

        allocState.totalAllocated -= size;
        allocState.typesAllocated[static_cast<size_t>(alloc)] -= size;

        BlitzenPlatform::PlatformFree(pBlock, false);
    }

    void BlitMemoryCopy(void* pDst, void* pSrc, size_t size)
    {
        BlitzenPlatform::PlatformMemCopy(pDst, pSrc, size);
    }

    void BlitMemorySet(void* pDst, int32_t value, size_t size)
    {
        BlitzenPlatform::PlatformMemSet(pDst, value, size);
    }
    
    void BlitMemoryZero(void* pBlock, size_t size)
    {
        BlitzenPlatform::PlatformMemZero(pBlock, size);
    }
}