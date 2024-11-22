#pragma once

#include <iostream>

#include "BlitzenVulkan/vulkanRenderer.h"

//Has data that glfw will need to access on callback functions
struct WindowData
{
    GLFWwindow* pWindow;
    int width = 800;
    int height = 650;
    const char* title = "Blitzen0.X";
    bool bWindowShouldClose = false;
    bool bWindowResizeRequested = false;
    double currentMouseX = 0;
    double currentMouseY = 0;
    //This will be used to call functions set by the user for certain inputs
    BlitzenEngine::Controller* pController;
};

namespace BlitzenEngine
{
    class MainEngine
    {
    public:
        MainEngine();

        void MainEngineLoop();

        ~MainEngine();
    
    private:

        //Creates the window with glfw
        void glfwWindowInit();
    
    private:
        BlitzenVulkan::VulkanRenderer m_vulkan;

        Camera m_mainCamera;

        Controller m_mainController;

        float m_deltaTime = 0;
        float m_frameTime = 0;

        WindowData m_windowData;
    };
}