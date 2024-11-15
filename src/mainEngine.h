#pragma once

#include <iostream>

#include "BlitzenVulkan/vulkanRenderer.h"

#include "Game/camera.h"

namespace BlitzenEngine
{
    class MainEngine
    {
    public:
        MainEngine();

        void MainEngineLoop();

        ~MainEngine();
    
    private:
        BlitzenVulkan::VulkanRenderer m_vulkan;

        bool* m_pbEngineShouldTerminate = nullptr;

        Camera m_mainCamera;

        Controller m_mainController;

        float m_deltaTime = 0;
        float m_frameTime = 0;
    };
}