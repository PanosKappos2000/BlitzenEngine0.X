#pragma once

#include "Platform/blitPlatform.h"

#include "BlitzenVulkan/vulkanRenderer.h"

#define BLITZEN_VERSION                 "Blitzen Engine 0.X"

#define BLITZEN_VULKAN                  1

#define BLITZEN_WINDOW_STARTING_X       100
#define BLITZEN_WINDOW_STARTING_Y       100
#define BLITZEN_WINDOW_WIDTH            1280
#define BLITZEN_WINDOW_HEIGHT           720

namespace BlitzenEngine
{
    struct PlatformData
    {
        uint32_t windowWidth = BLITZEN_WINDOW_WIDTH;
        uint32_t windowHeight = BLITZEN_WINDOW_HEIGHT;
    };

    struct Clock
    {
        double startTime;
        double elapsed;
    };

    class MainEngine
    {
    public:
        MainEngine();

        void MainEngineLoop();

        ~MainEngine();
    
    private:

        void StartClock();
        void StopClock();
    
    private:

        BlitzenVulkan::VulkanRenderer m_vulkan;

        BlitzenPlatform::PlatformState platformState;
        PlatformData platformData;

        Camera m_mainCamera;

        Controller m_mainController;

        Clock m_clock;

        float m_deltaTime = 0;
        float m_frameTime = 0;

        uint8_t isRunning = 0;
    };
}