#include "MainEngine.h"

namespace BlitzenEngine
{
    Engine* Engine::m_pEngine;

    Engine::Engine()
    {
        m_pEngine = this;
        BLIT_INFO("%s booting", BLITZEN_VERSION)

        m_systems.eventSystem = BlitzenCore::EventsInit();
        BLIT_ASSERT_MESSAGE(m_systems.eventSystem, "Event system initalization failed! The Engine cannot start without the event system")

        BlitzenCore::InputInit();
        m_systems.inputSystem = 1;

        BLIT_ASSERT(BlitzenPlatform::PlatformStartup(&platformState, BLITZEN_VERSION, BLITZEN_WINDOW_STARTING_X, BLITZEN_WINDOW_STARTING_Y,
        platformData.windowWidth, platformData.windowHeight))

        BlitzenCore::RegisterEvent(BlitzenCore::BlitEventType::EngineShutdown, nullptr, OnEvent);
        BlitzenCore::RegisterEvent(BlitzenCore::BlitEventType::KeyPressed, nullptr, OnKeyPress);
        BlitzenCore::RegisterEvent(BlitzenCore::BlitEventType::KeyReleased, nullptr, OnKeyPress);
        BlitzenCore::RegisterEvent(BlitzenCore::BlitEventType::WindowResize, nullptr, OnEvent);

        //Then initialize vulkan giving it the glfw window width and height
        m_vulkan.Init(&platformState, &(platformData.windowWidth), &(platformData.windowHeight));

        isRunning = 1;
    }

    void Engine::MainEngineLoop()
    {
        // Loads scenes and actually get ready to draw. Some of the stuff that this does should not normally be up to the renderer
        // TODO: Change this, so that the scene data is loaded by another system and this just passes data to the GPU
        m_vulkan.UploadDataToGPU();

        StartClock();
        double previousTime = m_clock.elapsed;
        m_clock.elapsed = BlitzenPlatform::GetAbsoluteTime() - m_clock.startTime;
        m_deltaTime = m_clock.elapsed - previousTime;

        m_mainCamera.Init(m_deltaTime, &(platformData.windowWidth), &(platformData.windowHeight));

        //Loops until an event occurs that causes the engine to terminate
        while(isRunning)
        {
            BlitzenPlatform::PlatformPumpMessages(&platformState);

            if (!isSuspended)
            {
                previousTime = m_clock.elapsed;
                m_clock.elapsed = BlitzenPlatform::GetAbsoluteTime() - m_clock.startTime;
                m_deltaTime = m_clock.elapsed - previousTime;

                //Camera is update after events have bee polled
                m_mainCamera.MoveCamera(static_cast<float>(m_deltaTime));
                //Draw frame after camera has been updated
                BlitzenVulkan::RenderContext renderContext;
                renderContext.pCamera = &(m_mainCamera);
                renderContext.bResize = platformData.resize;
                m_vulkan.DrawFrame(renderContext);
                platformData.resize = 0;
            }
        }

        StopClock();
    }

    void Engine::StartClock()
    {
        m_clock.startTime = BlitzenPlatform::GetAbsoluteTime();
        m_clock.elapsed = 0;
    }

    void Engine::StopClock()
    {
        m_clock.elapsed = 0;
    }

    Engine::~Engine()
    {
        BLIT_WARN("%s shutting down", BLITZEN_VERSION)

        m_systems.eventSystem = 0;
        BlitzenCore::EventsShutdown();

        m_systems.inputSystem = 0;
        BlitzenCore::InputShutdown();

        m_vulkan.CleanupResources();
        BlitzenPlatform::PlatformShutdown(&platformState);

        m_pEngine = nullptr;
        isRunning = 0;
    }





    uint8_t OnEvent(BlitzenCore::BlitEventType eventType, void* pSender, void* pListener, BlitzenCore::EventContext data)
    {
        if(eventType == BlitzenCore::BlitEventType::EngineShutdown)
        {
            BLIT_WARN("Engine shutdown event encountered!")
            BlitzenEngine::Engine::GetEngineInstancePointer()->RequestShutdown();
            return 1; 
        }
        if(eventType == BlitzenCore::BlitEventType::WindowResize)
        {
            uint32_t width = data.data.ui32[0];
            uint32_t height = data.data.ui32[1];
            Engine::GetEngineInstancePointer()->UpdateWindowSize(width, height);

            return 1;
        }

        return 0;
    }

    void Engine::UpdateWindowSize(uint32_t width, uint32_t height)
    {
        platformData.windowWidth = width;
        platformData.windowHeight = height;
        platformData.resize = 1;

        if (width == 0 || height == 0)
        {
            uint8_t isSuspended = 1;
            return;
        }
        uint8_t isSuspended = 0;
    }

    uint8_t OnKeyPress(BlitzenCore::BlitEventType eventType, void* pSender, void* pListener, BlitzenCore::EventContext data)
    {
        //Get the key pressed from the event context
        BlitzenCore::BlitKey key = static_cast<BlitzenCore::BlitKey>(data.data.ui16[0]);

        if(eventType == BlitzenCore::BlitEventType::KeyPressed)
        {
            switch(key)
            {
                case BlitzenCore::BlitKey::__ESCAPE:
                {
                    BlitzenCore::EventContext newContext = {};
                    BlitzenCore::FireEvent(BlitzenCore::BlitEventType::EngineShutdown, nullptr, newContext);
                    return 1;
                }
                case BlitzenCore::BlitKey::__W:
                {
                    Engine::GetEngineInstancePointer()->GetMainCamera().SetVelocityZ(-1);
                    break;
                }
                case BlitzenCore::BlitKey::__S:
                {
                    Engine::GetEngineInstancePointer()->GetMainCamera().SetVelocityZ(1);
                    break;
                }
                case BlitzenCore::BlitKey::__A:
                {
                    Engine::GetEngineInstancePointer()->GetMainCamera().SetVelocityX(-1);
                    break;
                }
                case BlitzenCore::BlitKey::__D:
                {
                    Engine::GetEngineInstancePointer()->GetMainCamera().SetVelocityX(1);
                    break;
                }
                case BlitzenCore::BlitKey::__F4:
                {
                    // TODO: Add variable logic to change from indirect to regular pipeline
                    break;
                }
                default:
                {
                    return 1;
                }
            }
        }
        else if(eventType == BlitzenCore::BlitEventType::KeyReleased)
        {
            switch(key)
            {
                case BlitzenCore::BlitKey::__W:
                case BlitzenCore::BlitKey::__S:
                {
                    Engine::GetEngineInstancePointer()->GetMainCamera().SetVelocityZ(0);
                    break;
                }
                case BlitzenCore::BlitKey::__A:
                case BlitzenCore::BlitKey::__D:
                {
                    Engine::GetEngineInstancePointer()->GetMainCamera().SetVelocityX(0);
                    break;
                }
            }
        }
        return 0;
    }

}

void main()
{
    BlitzenCore::MemoryManagementInit();

    {
        BlitzenEngine::Engine engine;
        engine.MainEngineLoop();
    }

    BlitzenCore::MemoryManagementShutdown();
}