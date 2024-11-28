#include "blitMemory.h"
#include "Platform/blitPlatform.h"

namespace BlitzenCore
{
    static AllocationData allocState;

    void MemoryManagementInit()
    {
        BlitzenPlatform::PlatformMemZero(&allocState, sizeof(AllocationData));
    }

    void MemoryManagementShutdown()
    {
        /*if (BlitzenEngine::Engine::GetEngineInstancePointer())
        {
            BLIT_ERROR("Blitzen is still active, memory management cannot be shutdown")
            return;
        }*/
        BLIT_ASSERT_MESSAGE(!allocState.totalAllocated, "There is still unallocated memory")
    }
}