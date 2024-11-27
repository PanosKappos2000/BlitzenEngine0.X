#include "MainEngine.h"

namespace BlitzenEngine
{
    MainEngine::MainEngine()
    {
        BLIT_INFO("%s booting", BLITZEN_VERSION)

        BLIT_ASSERT(BlitzenPlatform::PlatformStartup(&platformState, BLITZEN_VERSION, BLITZEN_WINDOW_STARTING_X, BLITZEN_WINDOW_STARTING_Y, 
        platformData.windowWidth, platformData.windowHeight))

        //Then initialize vulkan giving it the glfw window width and height
        m_vulkan.Init(&platformState, &(platformData.windowWidth), &(platformData.windowHeight));
        m_mainCamera.Init(&m_deltaTime, &(platformData.windowWidth), &(platformData.windowHeight));

        isRunning = 1;
    }

    void MainEngine::MainEngineLoop()
    {
        // Loads scenes and actually get ready to draw. Some of the stuff that this does should not normally be up to the renderer
        // TODO: Change this, so that the scene data is loaded by another system and this just passes data to the GPU
        m_vulkan.UploadDataToGPU();

        StartClock();
        double previousTime = m_clock.elapsed;

        //Loops until an event occurs that causes the engine to terminate
        while(isRunning)
        {
            m_clock.elapsed = BlitzenPlatform::GetAbsoluteTime() - m_clock.startTime;
            m_frameTime = m_clock.elapsed - previousTime;
            previousTime = m_clock.elapsed;

            //Camera is update after events have bee polled
            m_mainCamera.MoveCamera();
            //Draw frame after camera has been updated
            m_vulkan.DrawFrame(m_mainCamera);
        }

        StopClock();
    }

    void MainEngine::StartClock()
    {
        m_clock.startTime = BlitzenPlatform::GetAbsoluteTime();
        m_clock.elapsed = 0;
    }

    void MainEngine::StopClock()
    {
        m_clock.elapsed = 0;
    }

    MainEngine::~MainEngine()
    {
        BLIT_WARN("%s shutting down", BLITZEN_VERSION)
        m_vulkan.CleanupResources();
        BlitzenPlatform::PlatformShutdown(&platformState);
    }

}

void main()
{
    BlitzenEngine::MainEngine engine;
    engine.MainEngineLoop();
}