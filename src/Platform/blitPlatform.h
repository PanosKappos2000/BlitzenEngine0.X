#pragma once

#include "Core/blitLogger.h"
#include "vulkan/vulkan.h"

#if _MSC_VER
    #define VULKAN_SURFACE_KHR_EXTENSION_NAME       "VK_KHR_win32_surface"
#elif linux
    #define VULKAN_SURFACE_KHR_EXTENSION_NAME       //I don't know yet, I will look it up later
#endif

namespace BlitzenPlatform
{
    struct PlatformState
    {
        // Will be used by each platform to allocate the right struct they want to use
        void* pInternalState;
    };

    uint8_t PlatformStartup(PlatformState* pState, const char* appName, int32_t initialX, int32_t initialY, uint32_t windowWidth, uint32_t windowHeight);
    void PlatformShutdown(PlatformState* pState);

    uint8_t PlatformPumpMessages(PlatformState* pState);

    /* -------------------------------------------------------------------------------------------------------
        These will not be called by systems directly, they're meant to aid the custom allocation functions, 
        since some memory functionality might be platform specific
    --------------------------------------------------------------------------------------------------------  */
    void* PlatformMalloc(size_t size, uint8_t aligned);
    void PlatformFree(void* pBlock, uint8_t aligned);
    void* PlatformMemZero(void* pBlock, size_t size);
    void* PlatformMemCopy(void* pDst, void* pSrc, size_t size);
    void* PlatformMemSet(void* pDst, int32_t value, size_t size);

    void ConsoleWrite(const char* message, uint8_t color);
    void ConsoleError(const char* message, uint8_t color);

    // This is basically like glfwGetTime()
    double GetAbsoluteTime();

    void PSleep(uint64_t ms);

    void CreateVulkanSurface(PlatformState* pState, VkInstance& instance, VkSurfaceKHR& surface, VkAllocationCallbacks* pAllocator);
}