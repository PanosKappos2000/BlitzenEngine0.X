#pragma once

#include "Platform/blitPlatform.h"
#include "Core/blitzenContainerLibrary.h"
#include "Core/blitEvents.h"

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
        uint8_t resize = 0;
    };

    struct Clock
    {
        double startTime;
        double elapsed;
    };

    struct EngineSystems
    {
        uint8_t loggingSystem = 0;

        uint8_t eventSystem = 0;
        // This will be held by the engine and not a static variable, so that the dynamic arrays can be cleaned up on shutdown
        BlitzenCore::EventSystemState eventSystemState;

        uint8_t inputSystem = 0;
    };

    class Engine
    {
    public:
        Engine();

        void MainEngineLoop();

        ~Engine();
        inline void RequestShutdown() { isRunning = 0; }

        inline static Engine* GetEngineInstancePointer() {return m_pEngine;}
        inline EngineSystems& GetEngineSystems() {return m_systems;}

        void UpdateWindowSize(uint32_t width, uint32_t height);

        inline Camera& GetMainCamera() { return m_mainCamera; }
    
    private:

        void StartClock();
        void StopClock();
    
    private:

        static Engine* m_pEngine;

        BlitzenVulkan::VulkanRenderer m_vulkan;

        BlitzenPlatform::PlatformState platformState;
        PlatformData platformData;

        EngineSystems m_systems;

        Camera m_mainCamera;

        Controller m_mainController;

        Clock m_clock;

        double m_deltaTime = 0;

        uint8_t isRunning = 0;
        uint8_t isSuspended = 0;
    };


    uint8_t OnEvent(BlitzenCore::BlitEventType eventType, void* pSender, void* pListener, BlitzenCore::EventContext data);

    uint8_t OnKeyPress(BlitzenCore::BlitEventType eventType, void* pSender, void* pListener, BlitzenCore::EventContext data);
}