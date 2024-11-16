#pragma once

#include "vulkanData.h"
#include "vulkanPipelines.h"

#include "VkBootstrap.h"

//I really don't like including gameplay elements in the renderer, I want to fix this in the future
#include "Input/controller.h"

//Libraries used to load assets like meshes and textures
#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"

namespace BlitzenVulkan
{
    #define BLITZEN_VULKAN_MAX_FRAMES_IN_FLIGHT  2

    //TODO: Move this to main engine(give it a better name as well) and give the renderer a more compact window data struct
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

    //Everything that uses VkBootstrap for initalization (except for the VkDevice which is a frequently used component)
    struct VkBootstrapObjects
    {
        VkInstance instance = VK_NULL_HANDLE;
        //Validation layers are only active in debug mode
        #ifndef NDEBUG
            const bool bUseValidationLayers = true;
        #else
            const bool bUseValidationLayers = false;
        #endif
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        //Although not initalized with VkBootstrap, it needs to be created after the instance and before choosing the gpu
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkPhysicalDevice chosenGPU = VK_NULL_HANDLE;

        //Swapchain objects, crucial for presenting renders on the window
        VkSwapchainKHR swapchain;
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkExtent2D swapchainExtent;
        VkFormat swapchainImageFormat;
    };

    //These are initialized with VkBootstrap but they are more frequently used than the other objects, so they stand alone
    struct Queues
    {
        VkQueue graphicsQueue;
        uint32_t graphicsIndex;

        VkQueue presentQueue;
        uint32_t presentIndex;

        VkQueue computeQueue;
        uint32_t computeIndex;
    };

    //This struct is used for commands outside the frame loop, while still in the initialization stage
    struct QuickCommands
    {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    };

    //Holds the tools that should have one copy for each frame that can be in flight
    struct FrameTools
    {
        VkCommandPool graphicsCommandPool;
        VkCommandBuffer graphicsCommandBuffer;

        VkSemaphore imageAcquiredSemaphore;
        VkSemaphore readyToPresentSemaphore;
        VkFence frameCompleteFence;

        DescriptorAllocator sceneDataDescriptroAllocator;
        AllocatedBuffer sceneDataUniformBuffer;
    };

    class VulkanRenderer
    {
    public:
        void Init();

        void DrawFrame();

        void CleanupResources();

    private:

        //Initializes glfw and the window that the renderer will render into
        void glfwWindowInit();

        //Initializes the objects that are part of VkBootstrapObjects using the vkBootstrap library
        void VkBootstrapInitializationHelp();

        void InitAllocator();

        //Initialize the quick commands command pool and command buffer, so that commands can be recorded during initialization
        void InitCommands();

        //Command recording outside of a frame should happen between these two functions
        void StartRecordingCommands();
        void SubmitCommands();

        //Initalized an array of frame tools based on how many frames in flight are allowed by the renderer
        void InitFrameTools();

        //This function allocates an image using vma and the allocatedImage struct from vulkanData.h
        void AllocateImage(AllocatedImage& imageToAllocate, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool bMipMapped = false);

        //This function does the same as the above, but also uploads data to a buffer and copies it to the allocated image
        void AllocateImage(void* dataToCopy, AllocatedImage& imageToAllocate, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, 
        bool bMipMapped = false);

        //Allocates a buffer using VMA and the AllocatedBuffer struct
        void AllocateBuffer(AllocatedBuffer& bufferToAllocate, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);

        //Reocrds commands to change the layout of an image, to suit the next operation that should be excecuted on it
        void TransitionImageLayout(const VkCommandBuffer& recordingCDB, VkImage& imageToTransition, VkImageLayout oldLayout, VkImageLayout newLayout, 
        VkAccessFlags2 srcAccessFlag, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 dstAccessFlag, VkPipelineStageFlags2 dstStageMask);

        //Records commands to copy one image to another
        void CopyImageToImage(const VkCommandBuffer& recordingCDB, VkImage& srcImage, VkImage& dstImage, VkImageLayout srcImageLayout, 
        VkImageLayout dstImageLayout, VkExtent2D srcImageSize, VkExtent2D dstImageSize);

        //Creates some basic textures to use when texture loading is sketchy
        void InitPlaceholderTextures();

        //Creates some placeholder data for the pseudo material system
        void InitMainMaterialData();

        //Passes the vertices and indices of all the objects in the draw context to m_globalIndexAndVertexBuffer
        void UploadMeshBuffersToGPU(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

        //Passes the vkCmdDrawIndexedIndirect commands and world matrix of all the objects in the draw context to m_drawIndirectDataBuffer
        void UploadIndirectDrawBuffersToGPU(std::vector<DrawIndirectData>& dataToUpload);

        //Passes the material data to the descriptor set for one material instance
        void WriteMaterialData(MaterialInstance& materialInstance, MaterialResources& materialResources, MaterialPass pass);

        //Takes a filepath to a gltf scene and loads its data using fastglft library
        void LoadScene(std::string& filepath, const char* sceneName, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
        //Called from inside load scene, to load textures(uses stbi image)
        void LoadGltfImage(AllocatedImage& imageToLoad, fastgltf::Asset& gltfAsset, fastgltf::Image& gltfImage);

        //This is called so that when the window is resized, the swapchain can be recreated to fit the new size
        void BootstrapRecreateSwapchain();



        //Cleans up vulkan objects contained in frame tools struct
        void CleanupFrameTools();

        //Clean up everything that was initialized with the help of VkBootstrap
        void CleanupBootstrapInitializedObjects();

    public:

        WindowData m_windowData;

        //Holds some variables that change the way the renderer works
        VulkanStats stats;

        //Interfaces with the chosen gpu throughout the lifetime of the renderer
        VkDevice m_device = VK_NULL_HANDLE;

        //The VMA allocator will allocate all buffers and images used throughout the application
        VmaAllocator m_allocator;

        inline SceneData& GetSceneData() {return m_globalSceneData;}

    private:

        //Used to indicate which index of the frame tools list should be used
        uint32_t m_currentFrame = 0;

        VkBootstrapObjects m_bootstrapObjects;
        Queues m_queues;

        //Used when recording commands outside of the frame loop
        QuickCommands m_commands;

        std::array<FrameTools, BLITZEN_VULKAN_MAX_FRAMES_IN_FLIGHT> m_frameTools;

        //The main rendering attachments
        AllocatedImage m_drawingAttachment;
        AllocatedImage m_depthAttachment;
        VkExtent2D m_drawExtent;

        //Placeholder textures, used for materials that are loaded without textures
        AllocatedImage m_placeholderBlackTexture;
        AllocatedImage m_placeholderWhiteTexture;
        AllocatedImage m_placeholderGreyTexture;
        AllocatedImage m_placeholderErrorTexture;
        VkSampler m_placeholderLinearSampler;
        VkSampler m_placeholderNearestSampler;

        //Holds the pipelines used for each material instance and other data
        GlobalMaterialData m_mainMaterialData;

        //Global scene data that will be uploaded to the shaders once at the start of each frame
        SceneData m_globalSceneData;
        VkDescriptorSetLayout m_globalSceneDescriptorSetLayout;

        //Holds all the vertices and indices of every object in the world
        MeshBuffers m_globalIndexAndVertexBuffer;
        //Holds all the vkDrawIndexedIndirectCommand, the objects' world matrix and the buffer that will be used for vkCmdDrawIndexedIndirect
        AllocatedBuffer m_drawIndirectDataBuffer;

        //Holds all the scenes that have been loaded from a glb file
        std::unordered_map<std::string, LoadedScene> m_scenes;

        //Holds all the render objects that will be drawn each frame
        DrawContext m_mainDrawContext;
    };
}