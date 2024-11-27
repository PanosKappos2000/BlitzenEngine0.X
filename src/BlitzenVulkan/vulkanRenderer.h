#pragma once

#include "vulkanData.h"
#include "vulkanPipelines.h"

//I really don't like including gameplay elements in the renderer, I want to fix this in the future
#include "Input/controller.h"

//Libraries used to load assets like meshes and textures
#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/glm_element_traits.hpp"

#include "Game/camera.h"

namespace BlitzenVulkan
{
    #define BLITZEN_VULKAN_MAX_FRAMES_IN_FLIGHT  2

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
        bool hasGraphicsIndex = false;

        VkQueue presentQueue;
        uint32_t presentIndex;
        bool hasPresentIndex = false;

        VkQueue computeQueue;
        uint32_t computeIndex;
        bool hasComputeIndex = false;
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

        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            AllocatedBuffer indirectFrustumDataUniformBuffer;
        #endif

        //#ifndef NDEBUG
            VkQueryPool timestampQueryPool;
        //#endif
    };

    class VulkanRenderer
    {
    public:
        void Init(void* pState, uint32_t* pWidth, uint32_t* pHeight);

        void UploadDataToGPU();

        void DrawFrame(const BlitzenEngine::Camera& camera);

        void CleanupResources();

    private:

        void CreateSwapchain(uint32_t* pWidth, uint32_t* pHeight);

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

        //Creates compute pipelines that the graphics pipeline will depend on for specific work
        void InitComputePipelines();

        //Creates some placeholder data for the pseudo material system
        void InitMainMaterialData(uint32_t materialDescriptorCount);

        //Passes all the render data loaded from meshes, material and textures and passes them to global buffers
        void UploadGlobalBuffersToGPU(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, 
        std::vector<MaterialConstants>& materialConstants, std::vector<DrawIndirectData>& indirectDrawData);

        //Passes the material data to the descriptor set for one material instance(currently this only sets the pPipeline pointer in each material instance)
        void WriteMaterialData(MaterialInstance& materialInstance, MaterialPass pass);
        //Passes the material resources to the descriptor set that will allow access to them on the shader through render object index
        void UploadMaterialResourcesToGPU(std::vector<MaterialResources>& materialResources);

        //Takes a filepath to a gltf scene and loads its data using fastglft library
        void LoadScene(std::string& filepath, const char* sceneName, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, 
        std::vector<MaterialConstants>& MaterialConstants, std::vector<MaterialResources>& resources);
        //Called from inside load scene, to load textures(uses stbi image)
        void LoadGltfImage(AllocatedImage& imageToLoad, fastgltf::Asset& gltfAsset, fastgltf::Image& gltfImage);

        //This is called so that when the window is resized, the swapchain can be recreated to fit the new size
        void BootstrapRecreateSwapchain();



        //Cleans up vulkan objects contained in frame tools struct
        void CleanupFrameTools();

        //Clean up everything that was initialized with the help of VkBootstrap
        void CleanupBootstrapInitializedObjects();

    public:

        uint32_t* m_pWindowWidth;
        uint32_t* m_pWindowHeight;

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

        // Might use this later
        VkAllocationCallbacks* m_pCustomAllocator = nullptr;

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

        //Binds a pipeline which will be responsible for fustrum culling and readying the indirect commands
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            ComputePipelineData m_indirectCullingComputePipelineData;
        #endif

        //Holds the pipelines used for each material instance and other data
        GlobalMaterialData m_mainMaterialData;

        //Global scene data that will be uploaded to the shaders once at the start of each frame
        SceneData m_globalSceneData;
        //The descriptor set layout of the global scene data needs to be saved so that the descriptor set can be allocated each frame
        VkDescriptorSetLayout m_globalSceneDescriptorSetLayout;

        //Holds all the vertices and indices of every object in the world
        MeshBuffers m_globalIndexAndVertexBuffer;
        //Holds all the vkDrawIndexedIndirectCommand that will be used for the vkCmdDrawIndexedIndirect call
        AllocatedBuffer m_drawIndirectDataBuffer;
        //Holds all the material constants (color factor, metal rough factor etc) that will be used in the scene
        AllocatedBuffer m_globalMaterialConstantsBuffer;
        AllocatedBuffer m_surfaceFrustumCollisionBuffer;

        //Holds all the scenes that have been loaded from a glb file
        std::unordered_map<std::string, LoadedScene> m_scenes;

        //Holds all the render objects that will be drawn each frame
        DrawContext m_mainDrawContext;
    };
}