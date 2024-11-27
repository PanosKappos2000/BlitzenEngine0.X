#include "MainEngine.h"

namespace BlitzenEngine
{
    MainEngine::MainEngine()
    {
        std::cout << "Blitzen 0.X booting\n";

        //Create the window first
        glfwWindowInit();
        //Then initialize vulkan giving it the glfw window width and height
        m_vulkan.Init(m_windowData.pWindow, &(m_windowData.width), &(m_windowData.height));
        m_mainCamera.Init(&m_deltaTime, &(m_windowData.width), &(m_windowData.height));
        m_windowData.pController = &m_mainController;

        //Setting up some default inputs
        m_mainController.SetKeyPressFunction(GLFW_KEY_ESCAPE, GLFW_PRESS, [&]() {
            m_windowData.bWindowShouldClose = true;
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_TAB, GLFW_PRESS, [&]() {
            #if BLITZEN_START_VULKAN_WITH_INDIRECT
                m_vulkan.stats.drawIndirectMode = !m_vulkan.stats.drawIndirectMode;
                std::cout << m_vulkan.stats.drawIndirectMode ? "Indirect Drawing on\n" : "Indirect Drawing off\n";
            #endif
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_W, GLFW_PRESS, [&](){
            m_mainCamera.SetVelocityZ(-1);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_W, GLFW_RELEASE, [&](){
            m_mainCamera.SetVelocityZ(0);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_S, GLFW_PRESS, [&](){
            m_mainCamera.SetVelocityZ(1);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_S, GLFW_RELEASE, [&](){
            m_mainCamera.SetVelocityZ(0);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_A, GLFW_PRESS, [&](){
            m_mainCamera.SetVelocityX(-1);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_A, GLFW_RELEASE, [&](){
            m_mainCamera.SetVelocityX(0);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_D, GLFW_PRESS, [&](){
            m_mainCamera.SetVelocityX(1);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_D, GLFW_RELEASE, [&](){
            m_mainCamera.SetVelocityX(0);
        });
        m_mainController.SetCursorFunctionPointer([&](float x, float y) {
            m_mainCamera.RotateCamera(x, y);
        });
    }

    void MainEngine::MainEngineLoop()
    {
        /*
        Setting up glfw events
        */
        glfwSetWindowUserPointer(m_windowData.pWindow, reinterpret_cast<void*>(&(m_windowData)));
        glfwSetWindowCloseCallback(m_windowData.pWindow, [](GLFWwindow* pWindow){
            WindowData* pUserPointer = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(pWindow));
            pUserPointer->bWindowShouldClose = true;
        });
        glfwSetWindowSizeCallback(m_windowData.pWindow, [](GLFWwindow* pWindow, int windowWidth, int windowHeight){
            WindowData* pUserPointer = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(pWindow));
            pUserPointer->bWindowResizeRequested = true;
            pUserPointer->width = windowWidth;
            pUserPointer->height = windowHeight;
        });
        glfwSetKeyCallback(m_windowData.pWindow, [](GLFWwindow* pWindow, int key, int scancode, int action, int mods){
            WindowData* pUserPointer = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(pWindow));
            Controller* pController = pUserPointer->pController;
            //If the function pointer extists in map, call that function according to key press action, otherwise, do nothing
            if(pController->m_KeyFunctionPointers.find(key) != pController->m_KeyFunctionPointers.end())
            {
                pController->m_KeyFunctionPointers[key][action]();
            }
        });
        glfwSetCursorPosCallback(m_windowData.pWindow, [](GLFWwindow* pWindow, double xpos, double ypos) {
            WindowData* pUserPointer = reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(pWindow));
            Controller* pController = pUserPointer->pController;
            if (pController->m_pfnCursor)
            {
                pController->m_pfnCursor(static_cast<float>(xpos - pUserPointer->currentMouseX),
                    static_cast<float>(ypos - pUserPointer->currentMouseY));
            }
            pUserPointer->currentMouseX = xpos;
            pUserPointer->currentMouseY = ypos;
            });

        // Loads scenes and actually get ready to draw. Some of the stuff that this does should not normally be up to the renderer
        // TODO: Change this, so that the scene data is loaded by another system and this just passes data to the GPU
        m_vulkan.UploadDataToGPU();

        //Give frame time the current time before the loop starts, so that the delta time does not get a huge value at the start
        m_frameTime = static_cast<float>(glfwGetTime());

        //Loops until an event occurs that causes the engine to terminate
        while(!(m_windowData.bWindowShouldClose))
        {
            float updatedFrameTime = static_cast<float>(glfwGetTime());
            m_deltaTime = updatedFrameTime - m_frameTime;
            m_frameTime = updatedFrameTime;

            glfwPollEvents();
            //Camera is update after events have bee polled
            m_mainCamera.MoveCamera();
            //Draw frame after camera has been updated
            m_vulkan.DrawFrame(m_mainCamera, m_windowData.bWindowResizeRequested);

            //Makse sure this is set to false so that the renderer does not keep recreating the swapchain
            m_windowData.bWindowResizeRequested = false;
        }
    }

    MainEngine::~MainEngine()
    {
        std::cout << "Blitzen 0.X termination\n";
        m_vulkan.CleanupResources();
        glfwDestroyWindow(m_windowData.pWindow);
        glfwTerminate();
    }




    void MainEngine::glfwWindowInit()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_windowData.pWindow = glfwCreateWindow(m_windowData.width, m_windowData.height, m_windowData.title, nullptr, nullptr);
    }
}

void main()
{
    BlitzenEngine::MainEngine engine;
    engine.MainEngineLoop();
}