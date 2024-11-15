#include "MainEngine.h"

namespace BlitzenEngine
{
    MainEngine::MainEngine()
    {
        std::cout << "Blitzen 0.X booting\n";

        m_vulkan.Init();
        m_mainCamera.Init(&(m_vulkan.GetSceneData().viewMatrix), &m_deltaTime);
        m_vulkan.m_windowData.pController = &m_mainController;

        //Setting up some default inputs
        m_mainController.SetKeyPressFunction(GLFW_KEY_ESCAPE, GLFW_PRESS, [&]() {
            m_vulkan.m_windowData.bWindowShouldClose = true;
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_W, GLFW_PRESS, [&](){
            m_mainCamera.MoveCamera(glm::vec3(0.0f, 0.0f, -1.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_W, GLFW_REPEAT, [&](){
            m_mainCamera.MoveCamera(glm::vec3(0.0f, 0.0f, -1.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_W, GLFW_RELEASE, [&](){});
        m_mainController.SetKeyPressFunction(GLFW_KEY_S, GLFW_PRESS, [&](){
            m_mainCamera.MoveCamera(glm::vec3(0.0f, 0.0f, 1.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_S, GLFW_REPEAT, [&](){
            m_mainCamera.MoveCamera(glm::vec3(0.0f, 0.0f, 1.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_S, GLFW_RELEASE, [&](){});
        m_mainController.SetKeyPressFunction(GLFW_KEY_A, GLFW_PRESS, [&](){
            m_mainCamera.MoveCamera(glm::vec3(-1.0f, 0.0f, 0.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_A, GLFW_REPEAT, [&](){
            m_mainCamera.MoveCamera(glm::vec3(-1.0f, 0.0f, 0.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_A, GLFW_RELEASE, [&](){});
        m_mainController.SetKeyPressFunction(GLFW_KEY_D, GLFW_PRESS, [&](){
            m_mainCamera.MoveCamera(glm::vec3(1.0f, 0.0f, 0.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_D, GLFW_REPEAT, [&](){
            m_mainCamera.MoveCamera(glm::vec3(1.0f, 0.0f, 0.0f), 0.f, 0.f);
        });
        m_mainController.SetKeyPressFunction(GLFW_KEY_D, GLFW_RELEASE, [&](){});
        m_mainController.SetCursorFunctionPointer([&](float x, float y) {
            m_mainCamera.MoveCamera(glm::vec3(0.f, 0.f, 0.f), x, y);
        });
    }

    void MainEngine::MainEngineLoop()
    {
        /*
        Setting up glfw events
        */
        m_pbEngineShouldTerminate = &(m_vulkan.m_windowData.bWindowShouldClose);
        glfwSetWindowUserPointer(m_vulkan.m_windowData.pWindow, reinterpret_cast<void*>(&(m_vulkan.m_windowData)));
        glfwSetWindowCloseCallback(m_vulkan.m_windowData.pWindow, [](GLFWwindow* pWindow){
             BlitzenVulkan::WindowData* pUserPointer = reinterpret_cast<BlitzenVulkan::WindowData*>(glfwGetWindowUserPointer(pWindow));
             pUserPointer->bWindowShouldClose = true;
        });
        glfwSetWindowSizeCallback(m_vulkan.m_windowData.pWindow, [](GLFWwindow* pWindow, int windowWidth, int windowHeight){
             BlitzenVulkan::WindowData* pUserPointer = reinterpret_cast<BlitzenVulkan::WindowData*>(glfwGetWindowUserPointer(pWindow));
             pUserPointer->bWindowResizeRequested = true;
             pUserPointer->width = windowWidth;
             pUserPointer->height = windowHeight;
        });
        glfwSetKeyCallback(m_vulkan.m_windowData.pWindow, [](GLFWwindow* pWindow, int key, int scancode, int action, int mods){
             BlitzenVulkan::WindowData* pUserPointer = reinterpret_cast<BlitzenVulkan::WindowData*>(glfwGetWindowUserPointer(pWindow));
             Controller* pController = pUserPointer->pController;
             //If the function pointer extists in map, call that function according to key press action, otherwise, do nothing
             if(pController->m_KeyFunctionPointers.find(key) != pController->m_KeyFunctionPointers.end())
             {
                 pController->m_KeyFunctionPointers[key][action]();
             }
        });
        glfwSetCursorPosCallback(m_vulkan.m_windowData.pWindow, [](GLFWwindow* pWindow, double xpos, double ypos) {
            BlitzenVulkan::WindowData* pUserPointer = reinterpret_cast<BlitzenVulkan::WindowData*>(glfwGetWindowUserPointer(pWindow));
            Controller* pController = pUserPointer->pController;
            if (pController->m_pfnCursor)
            {
                pController->m_pfnCursor(static_cast<float>(xpos - pUserPointer->currentMouseX),
                    static_cast<float>(ypos - pUserPointer->currentMouseY));
            }
            pUserPointer->currentMouseX = xpos;
            pUserPointer->currentMouseY = ypos;
            });

        //Give frame time the current time before the loop starts, so that the delta time does not get a huge value at the start
        m_frameTime = static_cast<float>(glfwGetTime());

        //Loops until an event occurs that causes the engine to terminate
        while(!(*m_pbEngineShouldTerminate))
        {
            float updatedFrameTime = static_cast<float>(glfwGetTime());
            m_deltaTime = updatedFrameTime - m_frameTime;
            m_frameTime = updatedFrameTime;

             glfwPollEvents();
             m_vulkan.DrawFrame();
        }
    }

    MainEngine::~MainEngine()
    {
        std::cout << "Blitzen 0.X termination\n";
        m_vulkan.CleanupResources();
    }
}

void main()
{
    BlitzenEngine::MainEngine engine;
    engine.MainEngineLoop();
}