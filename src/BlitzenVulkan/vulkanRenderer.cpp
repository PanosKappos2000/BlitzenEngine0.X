#include "vulkanRenderer.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace BlitzenVulkan
{
    void VulkanRenderer::Init()
    {
        glfwWindowInit();
        VkBootstrapInitializationHelp();
        InitAllocator();
        InitCommands();

        //Setup rendering attachments(depth and color image)
        AllocateImage(m_drawingAttachment, 
        VkExtent3D{static_cast<uint32_t>(m_windowData.width), static_cast<uint32_t>(m_windowData.height), 1}, 
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
        AllocateImage(m_depthAttachment, 
        VkExtent3D{static_cast<uint32_t>(m_windowData.width), static_cast<uint32_t>(m_windowData.height), 1}, 
        VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        m_drawExtent.width = m_windowData.width;
        m_drawExtent.height = m_windowData.height;

        InitFrameTools();

        InitPlaceholderTextures();

        InitMainMaterialData();

        //Temporarily holds all vertices and indices, once every scene and asset is loaded, it will all be uploaded in tounified buffer
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::string testScene = "Assets/structure.glb";
        LoadScene(testScene, "structure", vertices, indices);
        //Update every node in the scene to be included in the draw context
        m_scenes["structure"].AddToDrawContext(glm::mat4(1.f), m_mainDrawContext);
        //Upload vertices and indices for all object to m_globalIndexAndVertexBuffer
        UploadMeshBuffersToGPU(vertices, indices);
        //Upload indirect draw commands and other draw indirect data to m_drawIndirectDataBuffer
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            UploadIndirectDrawBuffersToGPU(m_mainDrawContext.indirectData);
        #endif
    }

    void VulkanRenderer::glfwWindowInit()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_windowData.pWindow = glfwCreateWindow(m_windowData.width, m_windowData.height, m_windowData.title, nullptr, nullptr);
    }

    void VulkanRenderer::VkBootstrapInitializationHelp()
    {
        vkb::InstanceBuilder vkbInstanceBuilder;

        vkbInstanceBuilder.set_app_name("Blitzen0.X Renderer");
	    vkbInstanceBuilder.request_validation_layers(m_bootstrapObjects.bUseValidationLayers); 
	    vkbInstanceBuilder.use_default_debug_messenger();
	    vkbInstanceBuilder.require_api_version(1, 3, 0);

        //VkbInstance built to initialize instance and debug messenger
        auto vkbInstanceBuilderResult = vkbInstanceBuilder.build();
        vkb::Instance vkbInstance = vkbInstanceBuilderResult.value();

        //Instance reference from vulkan data initialized
        m_bootstrapObjects.instance = vkbInstance.instance;

        //Debug Messenger reference from vulkan data initialized
        m_bootstrapObjects.debugMessenger = vkbInstance.debug_messenger;

        //Creates window surface before device selection
        glfwCreateWindowSurface(m_bootstrapObjects.instance, m_windowData.pWindow, nullptr, &(m_bootstrapObjects.surface));

        //Setting desired vulkan 1.3 features
        VkPhysicalDeviceVulkan13Features vulkan13Features{};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        //Using dynamic rendering since the engine will not benefit from the VkRenderPass
        vulkan13Features.dynamicRendering = true;
        vulkan13Features.synchronization2 = true;

        //Setting desired vulkan 1.2 features
        VkPhysicalDeviceVulkan12Features vulkan12Features{};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        //Will allow us to create GPU pointers to access storage buffers
        vulkan12Features.bufferDeviceAddress = true;
        vulkan12Features.descriptorIndexing = true;

        VkPhysicalDeviceVulkan11Features vulkan11Features{};
        vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vulkan11Features.shaderDrawParameters = true;

        VkPhysicalDeviceFeatures vulkanFeatures{};
        vulkanFeatures.multiDrawIndirect = true;

        //vkbDeviceSelector built with reference to vkbInstance built earlier
        vkb::PhysicalDeviceSelector vkbDeviceSelector{ vkbInstance };
        vkbDeviceSelector.set_minimum_version(1, 3);
        vkbDeviceSelector.set_required_features_13(vulkan13Features);
        vkbDeviceSelector.set_required_features_12(vulkan12Features);
        vkbDeviceSelector.set_required_features_11(vulkan11Features);
        vkbDeviceSelector.set_required_features(vulkanFeatures);
        vkbDeviceSelector.set_surface(m_bootstrapObjects.surface);

        //Selecting the GPU and giving its value to the vkb::PhysicalDevice handle
        vkb::PhysicalDevice vkbPhysicalDevice = vkbDeviceSelector.select().value();

        //Saving the actual vulkan gpu handle 
        m_bootstrapObjects.chosenGPU = vkbPhysicalDevice.physical_device;

        //Setting up the vkDevice based on the chosen gpu
        vkb::DeviceBuilder vkbDeviceBuilder{ vkbPhysicalDevice };
        vkb::Device vkbDevice = vkbDeviceBuilder.build().value();
        //Saving the actual vulkan device
        m_device = vkbDevice.device;

        //Setting up the graphics queue and its queue family index
        m_queues.graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        m_queues.graphicsIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

        //Setting up the present queue and its queue family index
        m_queues.presentQueue = vkbDevice.get_queue(vkb::QueueType::present).value();
        m_queues.presentIndex = vkbDevice.get_queue_index(vkb::QueueType::present).value();

        //Setting up the present queue and its queue family index
        m_queues.computeQueue = vkbDevice.get_queue(vkb::QueueType::compute).value();
        m_queues.presentIndex = vkbDevice.get_queue_index(vkb::QueueType::compute).value();

        vkb::SwapchainBuilder vkSwapBuilder{ m_bootstrapObjects.chosenGPU, m_device, m_bootstrapObjects.surface };

        //Setting the desired image format
        m_bootstrapObjects.swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

        vkb::Result<vkb::Swapchain> vkbSwapBuilderResult = vkSwapBuilder.set_desired_format(VkSurfaceFormatKHR{ 
            m_bootstrapObjects.swapchainImageFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) //Setting the desrired surface format
        	.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)//Setting the present mode to be limited to the speed of the monitor
        	.set_desired_extent(m_windowData.width, m_windowData.height)
        	.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        	.build();

        //Building the vkb swapchain so that the swapchain data can be retrieved
        vkb::Swapchain vkbSwapchain = vkbSwapBuilderResult.value();

        //Setting the actual vulkan swapchain struct
        m_bootstrapObjects.swapchain = vkbSwapchain.swapchain;

        //Saving the swapchain's window extent
        m_bootstrapObjects.swapchainExtent = vkbSwapchain.extent;

        //Saving the swapchain images, they will be used each frame to present the render result to the swapchain
        m_bootstrapObjects.swapchainImages = vkbSwapchain.get_images().value();

        //Saving the swapchain image views, they will be used to access each swapchain image
        m_bootstrapObjects.swapchainImageViews = vkbSwapchain.get_image_views().value();
    }

    void VulkanRenderer::InitAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.device = m_device;
        allocatorInfo.instance = m_bootstrapObjects.instance;
        allocatorInfo.physicalDevice = m_bootstrapObjects.chosenGPU;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        vmaCreateAllocator(&allocatorInfo, &m_allocator);
    }

    void VulkanRenderer::InitCommands()
    {
        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        //Allows command buffers created by this pool to be reset manually
        commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        //Most of the commands executed by this command object can be handled by the graphics queue family of the GPU
        commandPoolInfo.queueFamilyIndex = m_queues.graphicsIndex;
        vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &(m_commands.commandPool));

        VkCommandBufferAllocateInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandBufferCount = 1;
        commandBufferInfo.commandPool = m_commands.commandPool;
        commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        vkAllocateCommandBuffers(m_device, &commandBufferInfo, &m_commands.commandBuffer);
    }

    void VulkanRenderer::StartRecordingCommands()
    {
        vkResetCommandBuffer(m_commands.commandBuffer, 0);
        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(m_commands.commandBuffer, &commandBufferBeginInfo);
    }

    void VulkanRenderer::SubmitCommands()
    {
        vkEndCommandBuffer(m_commands.commandBuffer);
        VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
        commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSubmitInfo.deviceMask = 0;
        commandBufferSubmitInfo.commandBuffer = m_commands.commandBuffer;
        VkSubmitInfo2 commandsSubmitInfo{};
        commandsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        commandsSubmitInfo.waitSemaphoreInfoCount = 0;
        commandsSubmitInfo.signalSemaphoreInfoCount = 0;
        commandsSubmitInfo.commandBufferInfoCount = 1;
        commandsSubmitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
        vkQueueSubmit2(m_queues.graphicsQueue, 1, &commandsSubmitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_queues.graphicsQueue);
    }

    void DescriptorAllocator::Init(const VkDevice* pDV)
    {
        //Create a random poolSize for the descriptor pool, since none was specified
        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].descriptorCount = 5;
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 5;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[2].descriptorCount = 5;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        m_pDevice = pDV;
        CreateDescriptorPool(static_cast<uint32_t>(poolSizes.size()), poolSizes.data());
        m_poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        m_pPoolSizes = poolSizes.data();
    }

    void DescriptorAllocator::Init(const VkDevice* pDV, uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes)
    {
        m_pDevice = pDV;
        CreateDescriptorPool(poolSizeCount, pPoolSizes);
        m_poolSizeCount = poolSizeCount;
        m_pPoolSizes = pPoolSizes;
    }

    void DescriptorAllocator::CreateDescriptorPool(uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes)
    {
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.maxSets = 1000;
        descriptorPoolInfo.poolSizeCount = poolSizeCount;
        descriptorPoolInfo.pPoolSizes = pPoolSizes;
        m_availablePools.push_back(VkDescriptorPool());
        vkCreateDescriptorPool(*m_pDevice, &descriptorPoolInfo, nullptr, &m_availablePools.back());
    }

    void DescriptorAllocator::AllocateDescriptorSet(VkDescriptorSetLayout* pLayouts, uint32_t setCount, VkDescriptorSet* pSet)
    {
        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
        descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocateInfo.descriptorSetCount = setCount;
        descriptorSetAllocateInfo.pSetLayouts = pLayouts;
        descriptorSetAllocateInfo.descriptorPool = GetDescriptorPool();

        VkResult allocationResult  = vkAllocateDescriptorSets(*m_pDevice, &descriptorSetAllocateInfo, pSet);

        if(allocationResult == VK_ERROR_OUT_OF_POOL_MEMORY || allocationResult == VK_ERROR_FRAGMENTED_POOL)
        {
            //Get the allocation out of the available pools and put it in the full pools
            m_fullPools.push_back(m_availablePools.back());
            m_availablePools.pop_back();

            //Tries a new descriptor pool
            descriptorSetAllocateInfo.descriptorPool = GetDescriptorPool();
            allocationResult = vkAllocateDescriptorSets(*m_pDevice, &descriptorSetAllocateInfo, pSet);
        }

        //At this point, if the allocation does not succeed, something is wrong and the application should stop
        if(allocationResult != VK_SUCCESS)
        {
            __debugbreak();
        }
    }

    VkDescriptorPool DescriptorAllocator::GetDescriptorPool()
    {
        if(m_availablePools.empty())
        {
            CreateDescriptorPool(m_poolSizeCount, m_pPoolSizes);
        }   
        return m_availablePools.back();
    }

    void DescriptorAllocator::ResetDescriptorPools()
    {
        vkResetDescriptorPool(*m_pDevice, m_availablePools.back(), 0);
        while(!(m_fullPools.empty()))
        {
            vkResetDescriptorPool(*m_pDevice, m_fullPools.back(), 0);
            m_availablePools.push_back(m_fullPools.back());
            m_fullPools.pop_back();
        }
    }

    void VulkanRenderer::InitFrameTools()
    {
        VkCommandPoolCreateInfo commandPoolsInfo{};
        commandPoolsInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolsInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        commandPoolsInfo.queueFamilyIndex = m_queues.graphicsIndex;
        VkCommandBufferAllocateInfo commandBuffersInfo{};
        commandBuffersInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBuffersInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBuffersInfo.commandBufferCount = 1;

        VkFenceCreateInfo fencesInfo{};
        fencesInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fencesInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for(size_t i = 0; i < m_frameTools.size(); ++i)
        {
            vkCreateCommandPool(m_device, &commandPoolsInfo, nullptr, &(m_frameTools[i].graphicsCommandPool));
            //Each command pool will allocate a single command buffer
            commandBuffersInfo.commandPool = m_frameTools[i].graphicsCommandPool; 
            vkAllocateCommandBuffers(m_device, &commandBuffersInfo, &(m_frameTools[i].graphicsCommandBuffer));

            vkCreateFence(m_device, &fencesInfo, nullptr, &(m_frameTools[i].frameCompleteFence));
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &(m_frameTools[i].imageAcquiredSemaphore));
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &(m_frameTools[i].readyToPresentSemaphore));

            m_frameTools[i].sceneDataDescriptroAllocator.Init(&m_device);
            AllocateBuffer(m_frameTools[i].sceneDataUniformBuffer, sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            VMA_MEMORY_USAGE_CPU_TO_GPU);
        }
    }

    void VulkanRenderer::AllocateImage(AllocatedImage& imageToAllocate, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage,
    bool bMipMapped /* =false */)
    {
        imageToAllocate.extent = extent;
        imageToAllocate.format = format;

        VkImageCreateInfo imageToAllocateInfo{};
        imageToAllocateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        //Allows the gpu to shuffle the data for optimal performance
        imageToAllocateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageToAllocateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageToAllocateInfo.arrayLayers = 1;
        //No multisampling for now
        imageToAllocateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageToAllocateInfo.usage = usage;
        imageToAllocateInfo.extent = extent;
        imageToAllocateInfo.format= format;
        //Checks the bMipMapped parameter before initalizing mip levels member
        imageToAllocateInfo.mipLevels = (bMipMapped) ? 
        static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) : 1;

        //Allocation info after image create info
        VmaAllocationCreateInfo imageAllocationInfo{};
        imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        imageAllocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vmaCreateImage(m_allocator, &imageToAllocateInfo, &imageAllocationInfo, &(imageToAllocate.image), &(imageToAllocate.allocation), 
        nullptr);

        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = imageToAllocate.image;
        imageViewInfo.format = format;
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        //The image aspect that will be accesed depends on in the image is a depth or color attachment
        imageViewInfo.subresourceRange.aspectMask = (format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        vkCreateImageView(m_device, &imageViewInfo, nullptr, &(imageToAllocate.imageView));
    }

    void VulkanRenderer::AllocateImage(void* dataToCopy, AllocatedImage& imageToAllocate, VkExtent3D extent, VkFormat format, 
    VkImageUsageFlags usage, bool bMipMapped /* =false */)
    {
        //Create a buffer to use as a way to transfer the data to the image
        VkDeviceSize bufferSize = extent.width * extent.height * extent.depth * 4;
        AllocatedBuffer stagingBuffer;
        AllocateBuffer(stagingBuffer, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        //Pass the image data to the buffer
        memcpy(stagingBuffer.allocationInfo.pMappedData, dataToCopy, bufferSize);

        //Allocate the image with the original function
        AllocateImage(imageToAllocate, extent, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT, bMipMapped);

        StartRecordingCommands();
        //Transition the image to accept the transfer operation
        TransitionImageLayout(m_commands.commandBuffer, imageToAllocate.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE);

        VkBufferImageCopy2 imageToBufferCopyRegion{};
        imageToBufferCopyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
        imageToBufferCopyRegion.bufferOffset = 0;
        imageToBufferCopyRegion.bufferImageHeight = 0;
        imageToBufferCopyRegion.bufferRowLength = 0;
        imageToBufferCopyRegion.imageExtent = extent;
        //Specifies where to put the image data in the image object(for example for x it would be from offset.x to imageExtent.x + offset.x)
        imageToBufferCopyRegion.imageOffset = {0, 0, 0};
        imageToBufferCopyRegion.imageSubresource.mipLevel = 0;
        imageToBufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        imageToBufferCopyRegion.imageSubresource.layerCount = 1;
        imageToBufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkCopyBufferToImageInfo2 copyBufferToImageInfo{};
        copyBufferToImageInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
        copyBufferToImageInfo.srcBuffer = stagingBuffer.buffer;
        copyBufferToImageInfo.dstImage = imageToAllocate.image;
        copyBufferToImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyBufferToImageInfo.regionCount = 1;
        copyBufferToImageInfo.pRegions = &imageToBufferCopyRegion;
        vkCmdCopyBufferToImage2(m_commands.commandBuffer, &copyBufferToImageInfo);

        TransitionImageLayout(m_commands.commandBuffer, imageToAllocate.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE);

        SubmitCommands();

        vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    void VulkanRenderer::AllocateBuffer(AllocatedBuffer& bufferToAllocate, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, 
    VmaMemoryUsage memoryUsage)
    {
        VkBufferCreateInfo bufferToAllocateInfo{};
        bufferToAllocateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferToAllocateInfo.size = bufferSize;
        bufferToAllocateInfo.usage = bufferUsage;
        
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = memoryUsage;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        vmaCreateBuffer(m_allocator, &bufferToAllocateInfo, &allocationInfo, &bufferToAllocate.buffer, &bufferToAllocate.allocation, 
        &bufferToAllocate.allocationInfo);
    }

    void VulkanRenderer::TransitionImageLayout(const VkCommandBuffer& recordingCDB, VkImage& imageToTransition, VkImageLayout oldLayout, 
    VkImageLayout newLayout, VkAccessFlags2 srcAccessFlag, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 dstAccessFlag,
    VkPipelineStageFlags2 dstStageMask)
    {
        VkImageMemoryBarrier2 imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageMemoryBarrier.oldLayout = oldLayout;
        imageMemoryBarrier.newLayout = newLayout;
        imageMemoryBarrier.image = imageToTransition;
        
        imageMemoryBarrier.srcAccessMask = srcAccessFlag;
        imageMemoryBarrier.srcStageMask = srcStageMask;
        imageMemoryBarrier.dstAccessMask = dstAccessFlag;
        imageMemoryBarrier.dstStageMask = dstStageMask;

        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        //The image aspect that will be accesed depends on if the image is a depth or color attachment
        imageMemoryBarrier.subresourceRange.aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? 
        VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;
        vkCmdPipelineBarrier2(recordingCDB, &dependencyInfo);
    }

    void VulkanRenderer::CopyImageToImage(const VkCommandBuffer& recordingCDB, VkImage& srcImage, VkImage& dstImage, 
    VkImageLayout srcImageLayout, VkImageLayout dstImageLayout, VkExtent2D srcImageSize, VkExtent2D dstImageSize)
    {
        VkImageBlit2 imageBlitRegion{};
        imageBlitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

        imageBlitRegion.srcSubresource.baseArrayLayer = 0;
        imageBlitRegion.srcSubresource.layerCount = 1;
        imageBlitRegion.srcSubresource.mipLevel = 0;
        imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        
        imageBlitRegion.srcOffsets[1].x = srcImageSize.width;
        imageBlitRegion.srcOffsets[1].y = srcImageSize.height;
        imageBlitRegion.srcOffsets[1].z = 1;

        imageBlitRegion.dstSubresource.baseArrayLayer = 0;
        imageBlitRegion.dstSubresource.layerCount = 1;
        imageBlitRegion.dstSubresource.mipLevel = 0;
        imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        imageBlitRegion.dstOffsets[1].x = dstImageSize.width;
        imageBlitRegion.dstOffsets[1].y = dstImageSize.height;
        imageBlitRegion.dstOffsets[1].z = 1;

        VkBlitImageInfo2 blitImageInfo{};
        blitImageInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitImageInfo.srcImage = srcImage;
        blitImageInfo.srcImageLayout = srcImageLayout;
        blitImageInfo.dstImage = dstImage;
        blitImageInfo.dstImageLayout = dstImageLayout;
        blitImageInfo.regionCount = 1;
        blitImageInfo.pRegions = &imageBlitRegion;

        vkCmdBlitImage2(recordingCDB, &blitImageInfo);
    }

    void VulkanRenderer::InitPlaceholderTextures()
    {
        uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
        AllocateImage(reinterpret_cast<void*>(&white), m_placeholderBlackTexture, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);
        uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
        AllocateImage(reinterpret_cast<void*>(&grey), m_placeholderGreyTexture, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, 
        VK_IMAGE_USAGE_SAMPLED_BIT);
        uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
        AllocateImage(reinterpret_cast<void*>(&black), m_placeholderWhiteTexture, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, 
        VK_IMAGE_USAGE_SAMPLED_BIT);
        uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	    std::array<uint32_t, 16 *16 > pixels; 
	    for (int x = 0; x < 16; x++) 
        {
	    	for (int y = 0; y < 16; y++) 
            {
	    		pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
	    	}
	    }
        AllocateImage(reinterpret_cast<void*>(pixels.data()), m_placeholderErrorTexture, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, 
        VK_IMAGE_USAGE_SAMPLED_BIT);
        VkSamplerCreateInfo nearestSamplerInfo{};
        nearestSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        nearestSamplerInfo.magFilter = VK_FILTER_NEAREST;
        nearestSamplerInfo.minFilter = VK_FILTER_NEAREST;
        vkCreateSampler(m_device, &nearestSamplerInfo, nullptr, &m_placeholderNearestSampler);
        VkSamplerCreateInfo linearSamplerInfo{};
        linearSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        linearSamplerInfo.magFilter = VK_FILTER_LINEAR;
        linearSamplerInfo.minFilter = VK_FILTER_LINEAR;
        vkCreateSampler(m_device, &linearSamplerInfo, nullptr, &m_placeholderLinearSampler);
    }

    void VulkanRenderer::InitMainMaterialData()
    {
        GraphicsPipelineBuilder opaquePipelineBuilder;
        opaquePipelineBuilder.Init(&m_device, &(m_mainMaterialData.opaquePipeline.pipelineLayout), 
        &(m_mainMaterialData.opaquePipeline.graphicsPipeline));

        opaquePipelineBuilder.CreateShaderStage("VulkanShaders/MainGeometryShader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, "main");
        opaquePipelineBuilder.CreateShaderStage("VulkanShaders/MainGeometryShader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main");
        opaquePipelineBuilder.SetTriangleListInputAssembly();
        opaquePipelineBuilder.SetDynamicViewport();
        opaquePipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
        opaquePipelineBuilder.SetPrimitiveCulling(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        opaquePipelineBuilder.DisableDepthBias();
        opaquePipelineBuilder.DisableDepthClamp();
        opaquePipelineBuilder.DisableMultisampling();
        opaquePipelineBuilder.EnableDefaultDepthTest();
        opaquePipelineBuilder.DisableColorBlending();

        //Setting the binding for the scene data uniform buffer descriptor 
        VkDescriptorSetLayoutBinding sceneDataDescriptorBinding{};
        sceneDataDescriptorBinding.binding = 0;
        sceneDataDescriptorBinding.descriptorCount = 1;
        sceneDataDescriptorBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sceneDataDescriptorBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        //The descriptor set layout for the scene data will be one set with one binding
        VkDescriptorSetLayoutCreateInfo sceneDataDescriptorLayoutInfo{};
        sceneDataDescriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        sceneDataDescriptorLayoutInfo.bindingCount = 1;
        sceneDataDescriptorLayoutInfo.pBindings = &sceneDataDescriptorBinding;
        vkCreateDescriptorSetLayout(m_device, &sceneDataDescriptorLayoutInfo, nullptr, &m_globalSceneDescriptorSetLayout);

        //Setting the binding for the material constants, that define how a surface should react to different effects
        VkDescriptorSetLayoutBinding materialConstantsDescriptorBinding{};
        materialConstantsDescriptorBinding.binding = 0;
        materialConstantsDescriptorBinding.descriptorCount = 1;
        materialConstantsDescriptorBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        materialConstantsDescriptorBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        //Setting the binding for the combined image sampler for the base color texture of a material
        VkDescriptorSetLayoutBinding materialBaseColorTextureBinding{};
        materialBaseColorTextureBinding.binding = 1;
        materialBaseColorTextureBinding.descriptorCount = 1;
        materialBaseColorTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        materialBaseColorTextureBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        //Setting the binding for the combined image sampler for the metallic texture of a material
        VkDescriptorSetLayoutBinding materialMetallicTextureBinding{};
        materialMetallicTextureBinding.binding = 2;
        materialMetallicTextureBinding.descriptorCount = 1;
        materialMetallicTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        materialMetallicTextureBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        //Collecting all the material descriptor set layout bindings to pass to its layout
        std::array<VkDescriptorSetLayoutBinding, 3> materialDescriptorSetLayoutBindings = 
        {
            materialConstantsDescriptorBinding,
            materialBaseColorTextureBinding,
            materialMetallicTextureBinding
        };
        
        //Create the layout for the material data descriptor set
        VkDescriptorSetLayoutCreateInfo materialDescriptorSetLayoutCreateInfo{};
        materialDescriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        materialDescriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(materialDescriptorSetLayoutBindings.size());
        materialDescriptorSetLayoutCreateInfo.pBindings = materialDescriptorSetLayoutBindings.data();
        vkCreateDescriptorSetLayout(m_device, &materialDescriptorSetLayoutCreateInfo, nullptr, 
        &m_mainMaterialData.mainMaterialDescriptorSetLayout);

        std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = 
        {
            m_globalSceneDescriptorSetLayout, m_mainMaterialData.mainMaterialDescriptorSetLayout
        };

        //Setting the push constant that will be passed to the shader for every object to specify their model matrix
        VkPushConstantRange pushConstants{};
        pushConstants.offset = 0;
        pushConstants.size = sizeof(ModelMatrixPushConstant);
        pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        opaquePipelineBuilder.CreatePipelineLayout(static_cast<uint32_t>(descriptorSetLayouts.size()), descriptorSetLayouts.data(), 
        1, &pushConstants);

        opaquePipelineBuilder.Build();

        //Since indirect draw will have to use a different shader, a new pipeline needs to be created for it
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            opaquePipelineBuilder.m_pGraphicsPipeline = &(m_mainMaterialData.indirectDrawOpaqueGraphicsPipeline);
            opaquePipelineBuilder.m_pLayout = &(m_mainMaterialData.globalIndirectDrawPipelineLayout);

            opaquePipelineBuilder.CreateShaderStage("VulkanShaders/IndirectDrawShader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, "main");
            opaquePipelineBuilder.CreateShaderStage("VulkanShaders/MainGeometryShader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main");
            opaquePipelineBuilder.SetTriangleListInputAssembly();
            opaquePipelineBuilder.SetDynamicViewport();
            opaquePipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
            opaquePipelineBuilder.SetPrimitiveCulling(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            opaquePipelineBuilder.DisableDepthBias();
            opaquePipelineBuilder.DisableDepthClamp();
            opaquePipelineBuilder.DisableMultisampling();
            opaquePipelineBuilder.EnableDefaultDepthTest();
            opaquePipelineBuilder.DisableColorBlending();

            //The pipeline layout for indirect only uses the scene data descriptor set, to pass the scene data at the start of draw frame
            opaquePipelineBuilder.CreatePipelineLayout(1, &m_globalSceneDescriptorSetLayout, 0, nullptr);

            opaquePipelineBuilder.Build();
        #endif
    }

    void VulkanRenderer::UploadMeshBuffersToGPU(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
    {
        //Allocates the vertex buffer as a storage buffer that can accept data transfers and can retrive a device address
        VkDeviceSize vertexBufferSize = static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
        AllocateBuffer(m_globalIndexAndVertexBuffer.vertexBuffer, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        //Retrieving the address of the vertex buffer so that it can be accessed in the shader
        VkBufferDeviceAddressInfo vertexBufferAddressInfo{};
        vertexBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        vertexBufferAddressInfo.buffer = m_globalIndexAndVertexBuffer.vertexBuffer.buffer;
        m_globalIndexAndVertexBuffer.vertexBufferAddress = vkGetBufferDeviceAddress(m_device, &vertexBufferAddressInfo);

        //Allocates the index buffer as an index buffer that can accept data transfers
        VkDeviceSize indexBufferSize = static_cast<VkDeviceSize>(indices.size() * sizeof(uint32_t));
        AllocateBuffer(m_globalIndexAndVertexBuffer.indexBuffer, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | 
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        //Since the index and vertex buffer memory is GPU only, a staging buffer is required to pass them the cpu data
        AllocatedBuffer stagingBuffer;
        AllocateBuffer(stagingBuffer, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
        VMA_MEMORY_USAGE_CPU_TO_GPU);
        void* vertexAndIndexBufferData = stagingBuffer.allocation->GetMappedData();
        memcpy(vertexAndIndexBufferData, vertices.data(), static_cast<size_t>(vertexBufferSize));
        memcpy(reinterpret_cast<char*>(vertexAndIndexBufferData) + static_cast<size_t>(vertexBufferSize), indices.data(), 
        static_cast<size_t>(indexBufferSize));

        //With the vertex buffer and index buffer allocated and the data copied to a staging buffer, copy commands can be recorded
        StartRecordingCommands();

        VkBufferCopy2 vertexBufferCopyRegion{};
        vertexBufferCopyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        vertexBufferCopyRegion.size = vertexBufferSize;
        vertexBufferCopyRegion.srcOffset = 0;
        vertexBufferCopyRegion.dstOffset = 0;

        VkCopyBufferInfo2 vertexBufferCopy{};
        vertexBufferCopy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        vertexBufferCopy.srcBuffer = stagingBuffer.buffer;
        vertexBufferCopy.dstBuffer = m_globalIndexAndVertexBuffer.vertexBuffer.buffer;
        vertexBufferCopy.regionCount = 1;
        vertexBufferCopy.pRegions = &vertexBufferCopyRegion;

        vkCmdCopyBuffer2(m_commands.commandBuffer, &vertexBufferCopy);

        VkBufferCopy2 indexBufferCopyRegion{};
        indexBufferCopyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        indexBufferCopyRegion.size = indexBufferSize;
        indexBufferCopyRegion.srcOffset = vertexBufferSize;
        indexBufferCopyRegion.dstOffset = 0;

        VkCopyBufferInfo2 indexBufferCopy{};
        indexBufferCopy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        indexBufferCopy.srcBuffer = stagingBuffer.buffer;
        indexBufferCopy.dstBuffer = m_globalIndexAndVertexBuffer.indexBuffer.buffer;
        indexBufferCopy.regionCount = 1;
        indexBufferCopy.pRegions = &indexBufferCopyRegion;

        vkCmdCopyBuffer2(m_commands.commandBuffer, &indexBufferCopy);

        SubmitCommands();

        vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    void VulkanRenderer::UploadIndirectDrawBuffersToGPU(std::vector<DrawIndirectData>& dataToUpload)
    {
        //Allocate the buffer that will hold the indirect commands and the data needed for the objects
        VkDeviceSize bufferSize = sizeof(DrawIndirectData) * dataToUpload.size();
        AllocateBuffer(m_drawIndirectDataBuffer, bufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        //Store the device address of the indirect buffer, so that any data passed along with it can be accessed in the shader
        VkBufferDeviceAddressInfo allocatedBufferAddressInfo{};
        allocatedBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        allocatedBufferAddressInfo.buffer = m_drawIndirectDataBuffer.buffer;
        m_globalSceneData.indirectBufferAddress = vkGetBufferDeviceAddress(m_device, &allocatedBufferAddressInfo);

        //Put the data on a staging buffer, to use it to transfer the data to the GPU buffer
        AllocatedBuffer stagingBuffer;
        AllocateBuffer(stagingBuffer, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        void* drawIndirectDataHolder = stagingBuffer.allocation->GetMappedData(); 
        memcpy(drawIndirectDataHolder, dataToUpload.data(), bufferSize);

        StartRecordingCommands();

        VkBufferCopy2 copyBufferRegion{};
        copyBufferRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        copyBufferRegion.dstOffset = 0;
        copyBufferRegion.srcOffset = 0;
        copyBufferRegion.size = bufferSize;

        VkCopyBufferInfo2 copyBufferInfo{};
        copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        copyBufferInfo.dstBuffer = m_drawIndirectDataBuffer.buffer;
        copyBufferInfo.srcBuffer = stagingBuffer.buffer;
        copyBufferInfo.regionCount = 1;
        copyBufferInfo.pRegions = &copyBufferRegion;
        vkCmdCopyBuffer2(m_commands.commandBuffer, &copyBufferInfo);

        SubmitCommands();

        vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    void VulkanRenderer::WriteMaterialData(MaterialInstance& materialInstance, MaterialResources& materialResources, 
    MaterialPass pass)
    {
        if(pass == MaterialPass::Opaque)
        {
            materialInstance.pPipeline = &(m_mainMaterialData.opaquePipeline);
        }
        else if(pass == MaterialPass::Transparent)
        {
            materialInstance.pPipeline = &(m_mainMaterialData.opaquePipeline);
            std::cout << "No support for transparent objects yet\n";
            //materialInstance.pPipeline->graphicsPipeline = m_mainMaterialData.transparentPipeline.graphicsPipeline;
            //materialInstance.pPipeline->pipelineLayout = m_mainMaterialData.transparentPipeline.pipelineLayouts;
        }

        std::array<VkWriteDescriptorSet, 3> descriptorWrites = {};

        //Setting up a descriptor write on the buffer that stores material constants
        VkDescriptorBufferInfo materialConstantsBufferInfo{};
        materialConstantsBufferInfo.buffer = materialResources.dataBuffer;
        materialConstantsBufferInfo.offset = materialResources.dataBufferOffset;
        materialConstantsBufferInfo.range = sizeof(MaterialConstants);
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstSet = materialInstance.descriptorToBind;
        descriptorWrites[0].pBufferInfo = &materialConstantsBufferInfo;

        //Setting up a descriptor write on the combined image sampler for the base color texture
        VkDescriptorImageInfo baseColorTextureImageInfo{};
        baseColorTextureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        baseColorTextureImageInfo.imageView = materialResources.colorImage.imageView;
        baseColorTextureImageInfo.sampler = materialResources.colorSampler;
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstSet = materialInstance.descriptorToBind;
        descriptorWrites[1].pImageInfo = &baseColorTextureImageInfo;

        //Setting up a descriptor write on the combined image sampler for the metal rough texture
        VkDescriptorImageInfo metalRoughTextureImageInfo{};
        metalRoughTextureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        metalRoughTextureImageInfo.imageView = materialResources.metalRoughImage.imageView;
        metalRoughTextureImageInfo.sampler = materialResources.metalRoughSampler;
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstSet = materialInstance.descriptorToBind;
        descriptorWrites[2].pImageInfo = &metalRoughTextureImageInfo;


        vkUpdateDescriptorSets(m_device, 3, descriptorWrites.data(), 0, nullptr);
    }

    void VulkanRenderer::LoadScene(std::string& filepath, const char* sceneName, std::vector<Vertex>& vertices, 
    std::vector<uint32_t>& indices)
    {
        std::cout << "Loading GLTF: " << filepath << '\n';

        m_scenes[sceneName] = LoadedScene();
        LoadedScene& scene = m_scenes[sceneName];
        scene.m_pRenderer = this;

        fastgltf::Parser parser {};

        constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble 
        | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
        // fastgltf::Options::LoadExternalImages;

        fastgltf::GltfDataBuffer data;
        if(!data.loadFromFile(filepath))
        {
            std::cout << "failed to find filepath\n";
            //__debugbreak();
        }

        fastgltf::Asset gltf;

        std::filesystem::path path = filepath;

        auto type = fastgltf::determineGltfFileType(&data);
        if (type == fastgltf::GltfType::glTF) 
        {
            auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
            if (load) 
            {
                gltf = std::move(load.get());
            } 
            else 
            {
                std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
                return;
            }
        } 
        else if (type == fastgltf::GltfType::GLB) 
        {
            auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
            if (load) 
            {
                gltf = std::move(load.get());
            } 
            else 
            {
                std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
                return;
            }
        } 
        else 
        {
            std::cerr << "Failed to determine glTF container" << std::endl;
            return;
        }



        //It is known that a material has 1 unifrom buffer descriptor for material constants and 2 combined image samplers for textures
        std::array<VkDescriptorPoolSize, 2> materialDescriptorPoolSizes{};
        materialDescriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        materialDescriptorPoolSizes[0].descriptorCount = 1;
        materialDescriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        materialDescriptorPoolSizes[1].descriptorCount = 2;
        scene.m_descriptorAllocator.Init(&m_device, 2, materialDescriptorPoolSizes.data());


        /*
        Extracting the samplers for all textures in the gltf scene
        */
        for(fastgltf::Sampler& gltfSampler : gltf.samplers)
        {
            VkSamplerCreateInfo vulkanSamplerInfo{};
            vulkanSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            vulkanSamplerInfo.maxLod = VK_LOD_CLAMP_NONE;
            vulkanSamplerInfo.minLod = 0;

            //Get the mag filter from the gltf, if it doesn't have anything it defaults to nearest
            fastgltf::Filter gltfMagFilter = gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest);
            if(gltfMagFilter == fastgltf::Filter::Nearest || gltfMagFilter == fastgltf::Filter::NearestMipMapLinear 
            || gltfMagFilter == fastgltf::Filter::NearestMipMapNearest)
            {
                vulkanSamplerInfo.magFilter = VK_FILTER_NEAREST;
            }
            else
            {
                vulkanSamplerInfo.magFilter = VK_FILTER_LINEAR;
            }

            //Same thing happens with the min filter
            fastgltf::Filter gltfMinFilter = gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest);
            if(gltfMinFilter == fastgltf::Filter::Nearest || gltfMinFilter == fastgltf::Filter::NearestMipMapLinear 
            || gltfMinFilter == fastgltf::Filter::NearestMipMapNearest)
            {
                vulkanSamplerInfo.minFilter = VK_FILTER_NEAREST;
            }
            else
            {
                vulkanSamplerInfo.minFilter = VK_FILTER_LINEAR;
            }

            //Retrives the mip map mode in a similar fashion with the default being linear
            if(gltfMinFilter == fastgltf::Filter::NearestMipMapNearest || gltfMinFilter == fastgltf::Filter::LinearMipMapNearest)
            {
                vulkanSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            }
            else
            {
                vulkanSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            }

            //Puts a new vulkan sampler object at the back of the array in the gltf scene and initializes it with vkCreateSampler
            scene.m_samplers.emplace_back(VkSampler());
            vkCreateSampler(m_device, &vulkanSamplerInfo, nullptr, &(scene.m_samplers.back()));
        }

        //Since fastgltf uses indices, each part of the scene will be temporarily referenced by an array
        std::vector<MeshAsset*> meshAssets;
        std::vector<Node*> nodes;
        std::vector<AllocatedImage*> textureImages;
        std::vector<MaterialInstance*> materials;

        //Loading textures, only the renderer's default for now
        for(fastgltf::Image image : gltf.images)
        {
            if(image.name != "")
            {
                scene.m_textures[image.name.c_str()] = AllocatedImage();
                LoadGltfImage(scene.m_textures[image.name.c_str()], gltf, image);
                if (scene.m_textures[image.name.c_str()].image != VK_NULL_HANDLE)
                {
                    textureImages.push_back(&(scene.m_textures[image.name.c_str()]));
                }
                else
                {
                    scene.m_textures[image.name.c_str()].CleanupResources(m_device, m_allocator);
                    scene.m_textures.erase(image.name.c_str());
                    textureImages.push_back(&(m_placeholderErrorTexture));
                }
            }
            else
            {
                //Because some dirtbags don't name their textures, I have to do this crap
                std::string makeshiftName = std::to_string(static_cast<uint32_t>(textureImages.size()));
                scene.m_textures[makeshiftName] = AllocatedImage();
                LoadGltfImage(scene.m_textures[makeshiftName], gltf, image);
                if (scene.m_textures[makeshiftName].image != VK_NULL_HANDLE)
                {
                    textureImages.push_back(&(scene.m_textures[makeshiftName]));
                }
                else
                {
                    scene.m_textures[makeshiftName].CleanupResources(m_device, m_allocator);
                    scene.m_textures.erase(makeshiftName);
                    textureImages.push_back(&(m_placeholderErrorTexture));
                }
            }
            
        }

        //Allocate a buffer for material constants and resource and retrieve a pointer to its allocation
        AllocateBuffer(scene.m_materialDataBuffer, sizeof(MaterialConstants) * gltf.materials.size(), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        //Will be used to iterate through the memory of the Material constants buffer
        int materialDataIndex = 0;
        MaterialConstants* pMaterialConstants = reinterpret_cast<MaterialConstants*>(scene.m_materialDataBuffer.allocationInfo.pMappedData);

        /*Load materials*/
        for(fastgltf::Material& gltfMaterial : gltf.materials)
        {
            //Add a new entry in the materials of the scene and add a reference to it in the placeholder array
            scene.m_materials[gltfMaterial.name.c_str()] = MaterialInstance();
            materials.push_back(&(scene.m_materials[gltfMaterial.name.c_str()]));

            MaterialConstants materialConstants;
            //Get the base color data of the material
            materialConstants.colorFactors.x = gltfMaterial.pbrData.baseColorFactor[0];
            materialConstants.colorFactors.y = gltfMaterial.pbrData.baseColorFactor[1];
            materialConstants.colorFactors.z = gltfMaterial.pbrData.baseColorFactor[2];
            materialConstants.colorFactors.w = gltfMaterial.pbrData.baseColorFactor[3];
            //Get the metallic and rough factors of the material
            materialConstants.metalRoughFactors.x = gltfMaterial.pbrData.metallicFactor;
            materialConstants.metalRoughFactors.y = gltfMaterial.pbrData.roughnessFactor;
            //Pass all the data that was retrieved to the material buffer
            pMaterialConstants[materialDataIndex] = materialConstants;

            //Simple distinction between transparent and opaque objects, not doing anything with transparent materials for now
            MaterialPass passType = MaterialPass::Opaque;
            if (gltfMaterial.alphaMode == fastgltf::AlphaMode::Blend)
            {
                passType = MaterialPass::Transparent;
            }
       
            //Load placeholder resources for the material
            MaterialResources materialResources;
            materialResources.colorImage = m_placeholderGreyTexture;
            materialResources.colorSampler = m_placeholderLinearSampler;
            materialResources.metalRoughImage = m_placeholderGreyTexture;
            materialResources.metalRoughSampler = m_placeholderLinearSampler;

            materialResources.dataBuffer = scene.m_materialDataBuffer.buffer;
            //Getting the offset of the current material in the material buffer
            materialResources.dataBufferOffset = materialDataIndex * sizeof(MaterialConstants);

            if(gltfMaterial.pbrData.baseColorTexture.has_value())
            {
                //Get the index of the texture used by this material
                size_t materialColorImageIndex = gltf.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].
                imageIndex.value();
                size_t materialColorSamplerIndex = gltf.textures[gltfMaterial.pbrData.baseColorTexture.value().textureIndex].
                samplerIndex.value();

                materialResources.colorImage = *textureImages[materialColorImageIndex];
                materialResources.colorSampler = scene.m_samplers[materialColorSamplerIndex];
            }

            //Once all data has been retrieved, it can be passed to the descriptors
            scene.m_descriptorAllocator.AllocateDescriptorSet(&(m_mainMaterialData.mainMaterialDescriptorSetLayout), 1, 
            &(materials.back()->descriptorToBind));
            WriteMaterialData(scene.m_materials[gltfMaterial.name.c_str()], materialResources, passType);
            materialDataIndex++;
        }

        //Start iterating through all the meshes that were loaded from gltf
        for(size_t i = 0; i < gltf.meshes.size(); ++i)
        {
            //Add a new mesh to the vulkan mesh assets array and save its name
            scene.m_meshes[gltf.meshes[i].name.c_str()] = MeshAsset();
            meshAssets.push_back(&(scene.m_meshes[gltf.meshes[i].name.c_str()]));
            meshAssets.back()->assetName = gltf.meshes[i].name;
            //Iterate through all the surfaces in the mesh asset
            for (auto& primitive : gltf.meshes[i].primitives)
            {
                //Add a new geoSurface object at the back of the mesh assets array and update its indices
                meshAssets.back()->surfaces.push_back(GeoSurface());
                meshAssets.back()->surfaces.back().firstIndex = indices.size();
                meshAssets.back()->surfaces.back().indexCount = gltf.accessors[primitive.indicesAccessor.value()].count;

                size_t initialVertex = vertices.size();

                /* Load indices */
                fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) 
                    {
                        indices.push_back(idx + initialVertex);
                    });

                /* Load vertex positions */
                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newVertex;
                        newVertex.position = v;
                        newVertex.normal = { 1, 0, 0 };
                        newVertex.color = glm::vec4{ 1.f };
                        newVertex.uvMapX = 0;
                        newVertex.uvMapY = 0;
                        vertices[initialVertex + index] = newVertex;
                    });

                /* Load normals */
                auto normals = primitive.findAttribute("NORMAL");
                if (normals != primitive.attributes.end()) 
                {
                
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                        [&](glm::vec3 v, size_t index) 
                        {
                            vertices[initialVertex + index].normal = v;
                        });
                }

                /* Load uv maps */
                auto uv = primitive.findAttribute("TEXCOORD_0");
                if (uv != primitive.attributes.end()) {
                
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                        [&](glm::vec2 v, size_t index) 
                        {
                            vertices[initialVertex + index].uvMapX = v.x;
                            vertices[initialVertex + index].uvMapY = v.y;
                        });
                }

                /* Load vertex colors */
                auto colors = primitive.findAttribute("COLOR_0");
                if (colors != primitive.attributes.end())
                {
                
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                        [&](glm::vec4 v, size_t index) 
                        {
                            vertices[initialVertex + index].color = v;
                        });
                }
                //If the primitive has a material, it retrieves its index and saves it, otherwise it get the first material
                if (primitive.materialIndex.has_value())
                {
                    meshAssets.back()->surfaces.back().pMaterial = materials[primitive.materialIndex.value()];
                } 
                else 
                {
                    meshAssets.back()->surfaces.back().pMaterial = materials[0];
                }
            }

        }

        /* Load each node in the gltf scene */
        for (fastgltf::Node& node : gltf.nodes)
        {
            //Add a new entry to the scene's hash map and add a pointer to it in the nodes array
            scene.m_nodes[node.name.c_str()] = Node();
            nodes.push_back(&(scene.m_nodes[node.name.c_str()]));
            //Find the nodes with a mesh, and give them their mesh asset
            if(node.meshIndex.has_value())
            {
                nodes.back()->pMesh = meshAssets[*node.meshIndex];
            }

            //Update the local transofrm of each node
            std::visit(fastgltf::visitor { [&](fastgltf::Node::TransformMatrix matrix) {
                                          memcpy(&(nodes.back()->localTransform), matrix.data(), sizeof(matrix));
                                          },
                                          [&](fastgltf::Node::TRS transform) {
                                            glm::vec3 tl(transform.translation[0], transform.translation[1],
                                                transform.translation[2]);
                                            glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                                                transform.rotation[2]);
                                            glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                                            glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                                            glm::mat4 rm = glm::toMat4(rot);
                                            glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                                            nodes.back()->localTransform = tm * rm * sm;
                                            } }, node.transform);

        }

        //Having loaded all the nodes, iterate through them to update their parent-children relationships
        for (int i = 0; i < gltf.nodes.size(); ++i)
        {
            Node* pParent = nodes[i];
            for (auto& child : gltf.nodes[i].children)
            {
                //Add each child to the parent node
                pParent->m_children.push_back(nodes[child]);
                //Give every child a pointer to the parent node
                pParent->m_children.back()->pParent = pParent;
            }
        }

        //Iterates through them once more to find the no-parent nodes and update the transform of their children
        for(Node* node : nodes)
        {
            if(!(node->pParent))
            {
                //Add every node that does not have a parent to the top nodes and refresh its children's transform
                scene.m_pureParentNodes.push_back(node);
                node->UpdateTransform(glm::mat4(1.f));
            }
        }
    }

    void VulkanRenderer::LoadGltfImage(AllocatedImage& imageToLoad, fastgltf::Asset& gltfAsset, fastgltf::Image& gltfImage)
    {
        int width; 
        int height;
        int nrChannels;

        std::visit(
            fastgltf::visitor {
                [](auto& arg) {},
                [&](fastgltf::sources::URI& filePath) {
                    assert(filePath.fileByteOffset == 0); 
                    assert(filePath.uri.isLocalPath());                                                         
                    const std::string path(filePath.uri.path().begin(),
                        filePath.uri.path().end()); 
                    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                    if (data) {
                        VkExtent3D imagesize;
                        imagesize.width = width;
                        imagesize.height = height;
                        imagesize.depth = 1;

                        AllocateImage(data, imageToLoad, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);

                        stbi_image_free(data);
                    }
                },
                [&](fastgltf::sources::Vector& vector) {
                    unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                        &width, &height, &nrChannels, 4);
                    if (data) {
                        VkExtent3D imagesize;
                        imagesize.width = width;
                        imagesize.height = height;
                        imagesize.depth = 1;

                        AllocateImage(data, imageToLoad, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                        stbi_image_free(data);
                    }
                },
                [&](fastgltf::sources::BufferView& view) {
                    auto& bufferView = gltfAsset.bufferViews[view.bufferViewIndex];
                    auto& buffer = gltfAsset.buffers[bufferView.bufferIndex];

                    std::visit(fastgltf::visitor { 
                                   [](auto& arg) {},
                                   [&](fastgltf::sources::Vector& vector) {
                                       unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                           static_cast<int>(bufferView.byteLength),
                                           &width, &height, &nrChannels, 4);
                                       if (data) {
                                           VkExtent3D imagesize;
                                           imagesize.width = width;
                                           imagesize.height = height;
                                           imagesize.depth = 1;

                                           AllocateImage(data, imageToLoad, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                               VK_IMAGE_USAGE_SAMPLED_BIT,false);

                                           stbi_image_free(data);
                                       }
                                   } },
                        buffer.data);
                },
            },
            gltfImage.data);
    }
    

    /*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    This is where all rendering commands during the game loop occur.
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
    void VulkanRenderer::DrawFrame()
    {
        //If the window is resized, the swapchain needs to adapt
        if (m_windowData.bWindowResizeRequested)
        {
            BootstrapRecreateSwapchain();
        }

        /*-------------------------------
        Setting up the global scene data
        ---------------------------------*/
        //Setup the projection matrix
        m_globalSceneData.projectionMatrix = glm::perspective(glm::radians(70.f), (float)m_windowData.width /
            (float)m_windowData.height, 10000.f, 0.1f);
        //Invert the projection matrix so that it matches glm and objects are not drawn upside down
        m_globalSceneData.projectionMatrix[1][1] *= -1;
        m_globalSceneData.projectionViewMatrix = m_globalSceneData.projectionMatrix * m_globalSceneData.viewMatrix;
        //Default lighting parameters
        m_globalSceneData.ambientColor = glm::vec4(.1f);
        m_globalSceneData.sunlightColor = glm::vec4(1.f);
        m_globalSceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
        //Give the address of the shared vertex buffer to the global scene data
        m_globalSceneData.vertexBufferAddress = m_globalIndexAndVertexBuffer.vertexBufferAddress;

        //Wait for queue submition to signal the fence with a timeout of 1 second
        vkWaitForFences(m_device, 1, &(m_frameTools[m_currentFrame].frameCompleteFence), VK_TRUE, 1000000000);
        vkResetFences(m_device, 1, &(m_frameTools[m_currentFrame].frameCompleteFence));


        //Passing global scene data to the shaders
        m_frameTools[m_currentFrame].sceneDataDescriptroAllocator.ResetDescriptorPools();
        SceneData* pSceneData = reinterpret_cast<SceneData*>(m_frameTools[m_currentFrame].
        sceneDataUniformBuffer.allocation->GetMappedData());
        *pSceneData = m_globalSceneData;
        VkDescriptorSet sceneDataDescriptorSet;
        m_frameTools[m_currentFrame].sceneDataDescriptroAllocator.AllocateDescriptorSet(&m_globalSceneDescriptorSetLayout, 
        1, &sceneDataDescriptorSet);
        //With the descriptor set for this frame allocated, it's ready to be written
        VkDescriptorBufferInfo writeSceneDataUniformBuffer{};
        writeSceneDataUniformBuffer.buffer = m_frameTools[m_currentFrame].sceneDataUniformBuffer.buffer;
        writeSceneDataUniformBuffer.offset = 0;
        writeSceneDataUniformBuffer.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet writeSceneDataDescriptor{};
        writeSceneDataDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSceneDataDescriptor.descriptorCount = 1;
        writeSceneDataDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeSceneDataDescriptor.dstBinding = 0;
        writeSceneDataDescriptor.dstSet = sceneDataDescriptorSet;
        writeSceneDataDescriptor.pBufferInfo = &writeSceneDataUniformBuffer;
        vkUpdateDescriptorSets(m_device, 1, &writeSceneDataDescriptor, 0, nullptr);


        //Acquire the next image in the swapchain, so that rendering results can be presented on the window
        uint32_t swapchainImageIndex;
        vkAcquireNextImageKHR(m_device, m_bootstrapObjects.swapchain, 1000000000, m_frameTools[m_currentFrame].imageAcquiredSemaphore,
        VK_NULL_HANDLE, &swapchainImageIndex);

        //Return the command buffer to the initial state and then put it in the recording state
        vkResetCommandBuffer(m_frameTools[m_currentFrame].graphicsCommandBuffer, 0);
        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(m_frameTools[m_currentFrame].graphicsCommandBuffer, &commandBufferBeginInfo);

        /*--------------------------------------------------------------------------------------------
        After this point and until vkEndCommandBuffer is called, all graphics commands that need to be 
        called during the current frame will be recorded
        --------------------------------------------------------------------------------------------*/

        //Clearing the color of the drawing attachment image to get a dark, green-blue color on the window
        TransitionImageLayout(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_drawingAttachment.image, 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
        VkClearColorValue clearColorValue{};
        clearColorValue.float32[0] = 0.0f;
        clearColorValue.float32[1] = 0.9f;
        clearColorValue.float32[2] = 0.8f;
        clearColorValue.float32[3] = 0.8f;
        VkImageSubresourceRange clearColorImageSubresourceRange{};
        clearColorImageSubresourceRange.baseMipLevel = 0;
        clearColorImageSubresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        clearColorImageSubresourceRange.baseArrayLayer = 0;
        clearColorImageSubresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        clearColorImageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkCmdClearColorImage(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_drawingAttachment.image, VK_IMAGE_LAYOUT_GENERAL,
        &clearColorValue, 1, &clearColorImageSubresourceRange);

        //Transition the image layout of the attachments so that they are ready to begin rendering
        TransitionImageLayout(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_drawingAttachment.image, VK_IMAGE_LAYOUT_GENERAL, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        TransitionImageLayout(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_depthAttachment.image, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

        //Setting up the color attachment info
        VkRenderingAttachmentInfo renderingColorAttachment{};
        renderingColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        renderingColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        renderingColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        renderingColorAttachment.imageView = m_drawingAttachment.imageView;
        renderingColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        //Setting up the depth attachment info
        VkRenderingAttachmentInfo renderingDepthAttachment{};
        renderingDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        renderingDepthAttachment.clearValue.depthStencil.depth = 0.f;
        renderingDepthAttachment.clearValue.depthStencil.stencil = 0;
        renderingDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        renderingDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        renderingDepthAttachment.imageView = m_depthAttachment.imageView;
        renderingDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        //Setting up to begin rendering
        VkRenderingInfo mainRenderingInfo{};
        mainRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        mainRenderingInfo.viewMask = 0;
        mainRenderingInfo.layerCount = 1;
        mainRenderingInfo.renderArea.offset = {0, 0};
        mainRenderingInfo.renderArea.extent = m_drawExtent;
        mainRenderingInfo.colorAttachmentCount = 1;
        mainRenderingInfo.pColorAttachments = &renderingColorAttachment;
        mainRenderingInfo.pDepthAttachment = &renderingDepthAttachment;

        /*--------------------------------------------------------------------------------------------
        After this point and until vkCmdEndRendring is called, all rendering commands are recorded
        ---------------------------------------------------------------------------------------------*/

        vkCmdBeginRendering(m_frameTools[m_currentFrame].graphicsCommandBuffer, &mainRenderingInfo);

        //Bind the index buffer, it's the same for both the indirect and traditional version of the pipeline
        vkCmdBindIndexBuffer(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_globalIndexAndVertexBuffer.indexBuffer.buffer, 
        0, VK_INDEX_TYPE_UINT32);
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            //Bind the pipeline and the global descriptor set for the indirect pipeline if it is active
            if(stats.drawIndirectMode)
            {
                vkCmdBindPipeline(m_frameTools[m_currentFrame].graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                m_mainMaterialData.indirectDrawOpaqueGraphicsPipeline);
                vkCmdBindDescriptorSets(m_frameTools[m_currentFrame].graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                m_mainMaterialData.globalIndirectDrawPipelineLayout, 0, 1, &sceneDataDescriptorSet, 0, nullptr);
            }
            //Bind the pipeline and the global descriptor set for the traditional pipeline, if indirect is inactive
            else
            {
                vkCmdBindPipeline(m_frameTools[m_currentFrame].graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                m_mainMaterialData.opaquePipeline.graphicsPipeline);
                vkCmdBindDescriptorSets(m_frameTools[m_currentFrame].graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                m_mainMaterialData.opaquePipeline.pipelineLayout, 0, 1, &sceneDataDescriptorSet, 0, nullptr);
            }
        #else
            vkCmdBindPipeline(m_frameTools[m_currentFrame].graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_mainMaterialData.opaquePipeline.graphicsPipeline);
            vkCmdBindDescriptorSets(m_frameTools[m_currentFrame].graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_mainMaterialData.opaquePipeline.pipelineLayout, 0, 1, &sceneDataDescriptorSet, 0, nullptr);
        #endif
        //Since viewport and scissor were specified in the dynamic state, they now need to be set
        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(m_drawExtent.width);
        viewport.height = static_cast<float>(m_drawExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(m_frameTools[m_currentFrame].graphicsCommandBuffer, 0, 1, &viewport);
        VkRect2D scissor{};
        scissor.extent.width = m_drawExtent.width;
        scissor.extent.height = m_drawExtent.height;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        vkCmdSetScissor(m_frameTools[m_currentFrame].graphicsCommandBuffer, 0, 1, &scissor);

        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            if(stats.drawIndirectMode)
            {
                vkCmdDrawIndexedIndirect(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_drawIndirectDataBuffer.buffer, 
                offsetof(DrawIndirectData, indirectDraws), static_cast<uint32_t>(m_mainDrawContext.indirectData.size()), sizeof(DrawIndirectData));
            }
            //Go with the traditional method if draw indirect is inactive
            else
            {
                for(size_t i = 0; i < m_mainDrawContext.opaqueRenderObjects.size(); ++i)
                {
                    //The model/surface that is currently being worked on
                    RenderObject& opaque = m_mainDrawContext.opaqueRenderObjects[i];

                    ModelMatrixPushConstant pushConstant;
                    pushConstant.modelMatrix = opaque.modelMatrix;
                    vkCmdPushConstants(m_frameTools[m_currentFrame].graphicsCommandBuffer, opaque.pMaterial->pPipeline->pipelineLayout, 
                    VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrixPushConstant), &pushConstant);
                    vkCmdDrawIndexed(m_frameTools[m_currentFrame].graphicsCommandBuffer, opaque.indexCount, 1, opaque.firstIndex, 0, 0);
                }
            }
        #else
            for (size_t i = 0; i < m_mainDrawContext.opaqueRenderObjects.size(); ++i)
            {
                //The model/surface that is currently being worked on
                RenderObject& opaque = m_mainDrawContext.opaqueRenderObjects[i];

                ModelMatrixPushConstant pushConstant;
                pushConstant.modelMatrix = opaque.modelMatrix;
                vkCmdPushConstants(m_frameTools[m_currentFrame].graphicsCommandBuffer, opaque.pMaterial->pPipeline->pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelMatrixPushConstant), &pushConstant);
                vkCmdDrawIndexed(m_frameTools[m_currentFrame].graphicsCommandBuffer, opaque.indexCount, 1, opaque.firstIndex, 0, 0);
            }
        #endif
        /*-------------------------------
        End of rendering commands
        ---------------------------------*/
        vkCmdEndRendering(m_frameTools[m_currentFrame].graphicsCommandBuffer);

        //Transition the drawing attachment image for a transfer operation as the source and the swapchain image as the destination
        TransitionImageLayout(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_drawingAttachment.image, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
        VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT);
        TransitionImageLayout(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_bootstrapObjects.swapchainImages[swapchainImageIndex], 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, 
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT);
        CopyImageToImage(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_drawingAttachment.image, 
        m_bootstrapObjects.swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        m_drawExtent, m_bootstrapObjects.swapchainExtent);

        //Transition the image layout so that it can be used in swapchain presentation
        TransitionImageLayout(m_frameTools[m_currentFrame].graphicsCommandBuffer, m_bootstrapObjects.swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR , VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE);

        /*------------------------------------------------------------------------------------------------------------------------------
        All commands end here, and the command buffer will be put to the executable state and will be submitted to the graphics queue, 
        so that the graphics commands of this frame can be executed
        --------------------------------------------------------------------------------------------------------------------------------*/

        vkEndCommandBuffer(m_frameTools[m_currentFrame].graphicsCommandBuffer);

        //Initializing the info for the semaphore that tells commands to stop until a swapchain image is acquired
        VkSemaphoreSubmitInfo waitSemaphoreInfo{};
        waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreInfo.deviceIndex = 0;
        waitSemaphoreInfo.value = 1;
        waitSemaphoreInfo.semaphore = m_frameTools[m_currentFrame].imageAcquiredSemaphore;
        waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        //Initializing the info for the semaphore that tells commands that rendering commands are done and the frame is ready for presentation
        VkSemaphoreSubmitInfo signalSemaphoreInfo{};
        signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfo.deviceIndex = 0;
        signalSemaphoreInfo.value = 1;
        signalSemaphoreInfo.semaphore = m_frameTools[m_currentFrame].readyToPresentSemaphore;
        signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

        //Initializing the info for the graphics command buffer that will be submitted
        VkCommandBufferSubmitInfo graphicsCommandBufferInfo{};
        graphicsCommandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        graphicsCommandBufferInfo.commandBuffer = m_frameTools[m_currentFrame].graphicsCommandBuffer;
        graphicsCommandBufferInfo.deviceMask = 0;

        //Initialing the info for the entire submit operation, including the semaphores, command buffer and the fence that will be signalled at the end
        VkSubmitInfo2 graphicsCommandsSubmit{};
        graphicsCommandsSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        graphicsCommandsSubmit.waitSemaphoreInfoCount = 1;
        graphicsCommandsSubmit.pWaitSemaphoreInfos = &waitSemaphoreInfo;
        graphicsCommandsSubmit.signalSemaphoreInfoCount = 1;
        graphicsCommandsSubmit.pSignalSemaphoreInfos = &signalSemaphoreInfo;
        graphicsCommandsSubmit.commandBufferInfoCount = 1;
        graphicsCommandsSubmit.pCommandBufferInfos = &graphicsCommandBufferInfo;
        vkQueueSubmit2(m_queues.graphicsQueue, 1, &graphicsCommandsSubmit, m_frameTools[m_currentFrame].frameCompleteFence);

        //With the commands submitted to the queues, the frame presentation is ready to be set up
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &(m_frameTools[m_currentFrame].readyToPresentSemaphore);
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &(m_bootstrapObjects.swapchain);
        presentInfo.pImageIndices = &swapchainImageIndex;
        vkQueuePresentKHR(m_queues.presentQueue, &presentInfo);

        //The current frame gets incremented but does not go above the max frames in flight macro
        m_currentFrame = (m_currentFrame + 1) % BLITZEN_VULKAN_MAX_FRAMES_IN_FLIGHT;
    }
    /*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    End of drawFrame function
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/



    void VulkanRenderer::BootstrapRecreateSwapchain()
    {
        //Wait for the current frame to be done with the swapchain
        vkDeviceWaitIdle(m_device);

        //Destroy the current swapchain
        for(size_t i = 0; i < m_bootstrapObjects.swapchainImageViews.size(); ++i)
        {
            vkDestroyImageView(m_device, m_bootstrapObjects.swapchainImageViews[i], nullptr);
        }
        vkDestroySwapchainKHR(m_device, m_bootstrapObjects.swapchain, nullptr);

        //Rebuilding the swapchain in a similar fashion to VkBootstrapInitializationHelp
        vkb::SwapchainBuilder vkSwapBuilder{ m_bootstrapObjects.chosenGPU, m_device, m_bootstrapObjects.surface };
        vkb::Result<vkb::Swapchain> vkbSwapBuilderResult = vkSwapBuilder.set_desired_format(VkSurfaceFormatKHR{ 
            m_bootstrapObjects.swapchainImageFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        	.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            //The function assumes that the window width and height have been updated
        	.set_desired_extent(m_windowData.width, m_windowData.height)
        	.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        	.build();

        
        vkb::Swapchain vkbSwapchain = vkbSwapBuilderResult.value();
        m_bootstrapObjects.swapchain = vkbSwapchain.swapchain;
        m_bootstrapObjects.swapchainExtent = vkbSwapchain.extent;
        m_bootstrapObjects.swapchainImages = vkbSwapchain.get_images().value();
        m_bootstrapObjects.swapchainImageViews = vkbSwapchain.get_image_views().value();

        //The draw extent should also be updated depending on if the swapchain got bigger or smaller
        m_drawExtent.width = std::min(static_cast<uint32_t>(m_windowData.width), 
        static_cast<uint32_t>(m_drawingAttachment.extent.width));
        m_drawExtent.height = std::min(static_cast<uint32_t>(m_windowData.height), 
        static_cast<uint32_t>(m_drawingAttachment.extent.height));

        m_windowData.bWindowResizeRequested = false;
    }



    void VulkanRenderer::CleanupResources()
    { 
        vkDeviceWaitIdle(m_device);

        for(auto& [name, scene] : m_scenes)
        {
            scene.ClearAll();
        }

        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            vmaDestroyBuffer(m_allocator, m_drawIndirectDataBuffer.buffer, m_drawIndirectDataBuffer.allocation);
        #endif
        vmaDestroyBuffer(m_allocator, m_globalIndexAndVertexBuffer.vertexBuffer.buffer, 
        m_globalIndexAndVertexBuffer.vertexBuffer.allocation);
        vmaDestroyBuffer(m_allocator, m_globalIndexAndVertexBuffer.indexBuffer.buffer, 
        m_globalIndexAndVertexBuffer.indexBuffer.allocation);

        m_mainMaterialData.CleanupResources(m_device);
        vkDestroyDescriptorSetLayout(m_device, m_globalSceneDescriptorSetLayout, nullptr);

        m_placeholderBlackTexture.CleanupResources(m_device, m_allocator);
        m_placeholderWhiteTexture.CleanupResources(m_device, m_allocator);
        m_placeholderGreyTexture.CleanupResources(m_device, m_allocator);
        m_placeholderErrorTexture.CleanupResources(m_device, m_allocator);
        vkDestroySampler(m_device, m_placeholderLinearSampler, nullptr);
        vkDestroySampler(m_device, m_placeholderNearestSampler, nullptr);

        CleanupFrameTools();

        m_drawingAttachment.CleanupResources(m_device, m_allocator);
        m_depthAttachment.CleanupResources(m_device, m_allocator);

        vkDestroyCommandPool(m_device, m_commands.commandPool, nullptr);

        vmaDestroyAllocator(m_allocator);
        CleanupBootstrapInitializedObjects();

        glfwDestroyWindow(m_windowData.pWindow);
        glfwTerminate();
    }

    void DescriptorAllocator::CleanupResources()
    {
        for(VkDescriptorPool& pool : m_availablePools)
        {
            vkDestroyDescriptorPool(*m_pDevice, pool, nullptr);
        }

        for(VkDescriptorPool& pool : m_fullPools)
        {
            vkDestroyDescriptorPool(*m_pDevice, pool, nullptr);
        }
    }

    void GlobalMaterialData::CleanupResources(const VkDevice& device)
    {
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            vkDestroyPipeline(device, indirectDrawOpaqueGraphicsPipeline, nullptr);
            vkDestroyPipelineLayout(device, globalIndirectDrawPipelineLayout, nullptr);
        #endif
        vkDestroyPipeline(device, opaquePipeline.graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, opaquePipeline.pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, mainMaterialDescriptorSetLayout, nullptr);
    }

    void AllocatedImage::CleanupResources(const VkDevice& device, const VmaAllocator& allocator)
    {
        vmaDestroyImage(allocator, image, allocation);
        vkDestroyImageView(device, imageView, nullptr);
    }

    void VulkanRenderer::CleanupFrameTools()
    {
        for(size_t i = 0; i < m_frameTools.size(); ++i)
        {
            vkDestroyCommandPool(m_device, m_frameTools[i].graphicsCommandPool, nullptr);

            vkDestroyFence(m_device, m_frameTools[i].frameCompleteFence, nullptr);
            vkDestroySemaphore(m_device, m_frameTools[i].imageAcquiredSemaphore, nullptr);
            vkDestroySemaphore(m_device, m_frameTools[i].readyToPresentSemaphore, nullptr);

            m_frameTools[i].sceneDataDescriptroAllocator.CleanupResources();
            vmaDestroyBuffer(m_allocator, m_frameTools[i].sceneDataUniformBuffer.buffer, 
            m_frameTools[i].sceneDataUniformBuffer.allocation);
        }
    }

    void VulkanRenderer::CleanupBootstrapInitializedObjects()
    {
        for(size_t i = 0; i < m_bootstrapObjects.swapchainImageViews.size(); ++i)
        {
            vkDestroyImageView(m_device, m_bootstrapObjects.swapchainImageViews[i], nullptr);
        }
        vkDestroySwapchainKHR(m_device, m_bootstrapObjects.swapchain, nullptr);

        vkDestroyDevice(m_device, nullptr);

        vkDestroySurfaceKHR(m_bootstrapObjects.instance, m_bootstrapObjects.surface, nullptr);
        vkb::destroy_debug_utils_messenger(m_bootstrapObjects.instance, m_bootstrapObjects.debugMessenger, nullptr);
        vkDestroyInstance(m_bootstrapObjects.instance, nullptr);
    }
}