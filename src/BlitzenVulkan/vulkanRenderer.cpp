#include "vulkanRenderer.h"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace BlitzenVulkan
{

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

    void VulkanRenderer::UploadDataToGPU()
    {
        //Setup rendering attachments(depth and color image)
        AllocateImage(m_drawingAttachment, 
        VkExtent3D{static_cast<uint32_t>(*m_pWindowWidth), static_cast<uint32_t>(*m_pWindowHeight), 1}, 
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
        AllocateImage(m_depthAttachment, 
        VkExtent3D{static_cast<uint32_t>(*m_pWindowWidth), static_cast<uint32_t>(*m_pWindowHeight), 1}, 
        VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        m_drawExtent.width = *m_pWindowWidth;
        m_drawExtent.height = *m_pWindowHeight;

        InitPlaceholderTextures();



        //Temporarily holds all vertices, once every scene and asset is loaded, it will all be uploaded in a unified storage buffer
        std::vector<Vertex> vertices;
        //Temporarily holds all indices, once every scene and asset is loaded, it will all be uploaded in a unified index buffer
        std::vector<uint32_t> indices;
        //Temporarily holds all material constants, once every scene and asset is loaded, it will all be uploaded in a unified storage buffer
        std::vector<MaterialConstants> materialConstants;
        //Temporarily holds all material resources, once every scene and asset is loaded, it will all written to two uniform buffer arrays
        std::vector<MaterialResources> materialResources;
        std::string testScene = "Assets/structure.glb";
        std::string testScene2 = "Assets/Highpoly.glb";
        LoadScene(testScene, "structure", vertices, indices, materialConstants, materialResources);
        LoadScene(testScene2, "city", vertices, indices, materialConstants, materialResources);
        //Update every node in the scene to be included in the draw context
        m_scenes["structure"].AddToDrawContext(glm::mat4(1.f), m_mainDrawContext);
        m_scenes["city"].AddToDrawContext(glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 0)), m_mainDrawContext);
        #ifdef NDEBUG
        float iter = 1;
        for (float f = 0.f; f < 100.f; f += 1.f)
        {
            iter *= -1;
            m_scenes["city"].AddToDrawContext(glm::translate(glm::mat4(1.f), glm::vec3(-iter, iter, -f)), m_mainDrawContext);
        }
        for (float f = 0.f; f < 100.f; f += 1.f)
        {
            iter *= -1;
            m_scenes["city"].AddToDrawContext(glm::translate(glm::mat4(1.f), glm::vec3(iter, f, -iter)), m_mainDrawContext);
        }
        for (float f = 0.f; f < 100.f; f += 1.f)
        {
            iter *= -1;
            m_scenes["city"].AddToDrawContext(glm::translate(glm::mat4(1.f), glm::vec3(f, -iter, iter)), m_mainDrawContext);
        }
        #endif

        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            InitComputePipelines();
        #endif

        //This probably needs a better interface but it does its job for now
        InitMainMaterialData(static_cast<uint32_t>(materialResources.size()));

        InitFrameTools();
        
        //Upload all global buffers to GPU, if indirect is acitve, it will also upload the indirect commands
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            UploadGlobalBuffersToGPU(vertices, indices, materialConstants, m_mainDrawContext.indirectDrawData);
        #else
            std::vector<DrawIndirectData> emptyIndirect;
            UploadGlobalBuffersToGPU(vertices, indices, materialConstants, emptyIndirect);
        #endif
        //Write all material resources to the global descriptor set
        UploadMaterialResourcesToGPU(materialResources);
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

        //#ifndef NDEBUG
            VkQueryPoolCreateInfo queryPoolInfo{};
            queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolInfo.queryCount = 2;
            queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        //#endif

        for(size_t i = 0; i < m_frameTools.size(); ++i)
        {
            vkCreateCommandPool(m_device, &commandPoolsInfo, nullptr, &(m_frameTools[i].graphicsCommandPool));
            //Each command pool will allocate a single command buffer
            commandBuffersInfo.commandPool = m_frameTools[i].graphicsCommandPool; 
            vkAllocateCommandBuffers(m_device, &commandBuffersInfo, &(m_frameTools[i].graphicsCommandBuffer));

            vkCreateFence(m_device, &fencesInfo, nullptr, &(m_frameTools[i].frameCompleteFence));
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &(m_frameTools[i].imageAcquiredSemaphore));
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &(m_frameTools[i].readyToPresentSemaphore));

            //#ifndef NDEBUG
                vkCreateQueryPool(m_device, &queryPoolInfo, nullptr, &(m_frameTools[i].timestampQueryPool));
            //#endif

            VkDescriptorPoolSize sceneDataDescriptorPoolSize{};
            #if BLITZEN_START_VULKAN_WITH_INDIRECT
                sceneDataDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                sceneDataDescriptorPoolSize.descriptorCount = 2;
            #else
                sceneDataDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                sceneDataDescriptorPoolSize.descriptorCount = 1;
            #endif
            m_frameTools[i].sceneDataDescriptroAllocator.Init(&m_device);
            AllocateBuffer(m_frameTools[i].sceneDataUniformBuffer, sizeof(SceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            VMA_MEMORY_USAGE_CPU_TO_GPU);
            //The indirect version of Vulkan does frustum culling in the compute shader and needs the frustum data to be passed
            #if BLITZEN_START_VULKAN_WITH_INDIRECT
                AllocateBuffer(m_frameTools[i].indirectFrustumDataUniformBuffer, sizeof(glm::vec4) * 6, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                VMA_MEMORY_USAGE_CPU_TO_GPU);
            #endif
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

    void VulkanRenderer::InitComputePipelines()
    {
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            //Create a shader code for the compute shader spir-v code
            VkShaderModule computeShaderModule{};
            VkPipelineShaderStageCreateInfo pipelineShaderStage{};
            std::vector<char> shaderCode;
            CreateShaderProgram(m_device, "VulkanShaders/IndirectCulling.comp.glsl.spv", VK_SHADER_STAGE_COMPUTE_BIT, "main", computeShaderModule, 
            pipelineShaderStage, shaderCode);

            //Setting the binding for the scene data uniform buffer descriptor 
            VkDescriptorSetLayoutBinding sceneDataDescriptorBinding{};
            CreateDescriptorSetLayoutBinding(sceneDataDescriptorBinding, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            //The descriptor set layout for the scene data will be one set with one binding
            m_globalSceneDescriptorSetLayout = CreateDescriptorSetLayout(m_device, 1, &sceneDataDescriptorBinding);

            VkDescriptorSetLayoutBinding frustumCullingDescriptorSetLayoutBinding{};
            CreateDescriptorSetLayoutBinding(frustumCullingDescriptorSetLayoutBinding, 1, 6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
            VK_SHADER_STAGE_COMPUTE_BIT);
            m_indirectCullingComputePipelineData.setLayouts.resize(2);
            m_indirectCullingComputePipelineData.setLayouts[0] = CreateDescriptorSetLayout(m_device, 1, 
            &frustumCullingDescriptorSetLayoutBinding);
            m_indirectCullingComputePipelineData.setLayouts[1] = m_globalSceneDescriptorSetLayout;

            CreatePipelineLayout(m_device, &(m_indirectCullingComputePipelineData.layout), static_cast<uint32_t>(
            m_indirectCullingComputePipelineData.setLayouts.size()), m_indirectCullingComputePipelineData.setLayouts.data(), 
            0, nullptr);

            VkComputePipelineCreateInfo cullingPipelineInfo{};
            cullingPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            cullingPipelineInfo.stage = pipelineShaderStage;
            cullingPipelineInfo.layout = m_indirectCullingComputePipelineData.layout;

            vkCreateComputePipelines(m_device, nullptr, 1, &cullingPipelineInfo, nullptr, &(m_indirectCullingComputePipelineData.pipeline));

            vkDestroyShaderModule(m_device, computeShaderModule, nullptr);
        #endif
    }

    void VulkanRenderer::InitMainMaterialData(uint32_t materialDescriptorCount)
    {
        GraphicsPipelineBuilder opaquePipelineBuilder;
        opaquePipelineBuilder.Init(&m_device, &(m_mainMaterialData.opaquePipeline.pipelineLayout), 
        &(m_mainMaterialData.opaquePipeline.graphicsPipeline));

        //Set the fixed function states and the dynamic state
        opaquePipelineBuilder.CreateShaderStage("VulkanShaders/MainGeometryShader.vert.glsl.spv", VK_SHADER_STAGE_VERTEX_BIT, "main");
        opaquePipelineBuilder.CreateShaderStage("VulkanShaders/MainGeometryShader.frag.glsl.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main");
        opaquePipelineBuilder.SetTriangleListInputAssembly();
        opaquePipelineBuilder.SetDynamicViewport();
        opaquePipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
        opaquePipelineBuilder.SetPrimitiveCulling(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        opaquePipelineBuilder.DisableDepthBias();
        opaquePipelineBuilder.DisableDepthClamp();
        opaquePipelineBuilder.DisableMultisampling();
        opaquePipelineBuilder.EnableDefaultDepthTest();
        opaquePipelineBuilder.DisableColorBlending();

        //If blitzen sets up for indirect drawing the scene data descriptor set layout has already been created
        #if !BLITZEN_START_VULKAN_WITH_INDIRECT
            //Setting the binding for the scene data uniform buffer descriptor 
            VkDescriptorSetLayoutBinding sceneDataDescriptorBinding{};
            CreateDescriptorSetLayoutBinding(sceneDataDescriptorBinding, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
            //The descriptor set layout for the scene data will be one set with one binding
            m_globalSceneDescriptorSetLayout = CreateDescriptorSetLayout(m_device, 1, &sceneDataDescriptorBinding);
        #endif
        //Setting the binding for the combined image sampler for the base color texture of a material
        VkDescriptorSetLayoutBinding materialBaseColorTextureBinding{};
        CreateDescriptorSetLayoutBinding(materialBaseColorTextureBinding, 0, materialDescriptorCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        //Setting the binding for the combined image sampler for the metallic texture of a material
        VkDescriptorSetLayoutBinding materialMetallicTextureBinding{};
        CreateDescriptorSetLayoutBinding(materialMetallicTextureBinding, 1, materialDescriptorCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        //Collecting all the material descriptor set layout bindings to pass to its layout
        std::array<VkDescriptorSetLayoutBinding, 2> materialDescriptorSetLayoutBindings = 
        {
            materialBaseColorTextureBinding,
            materialMetallicTextureBinding
        };
        //Create the layout for the material data descriptor set
        m_mainMaterialData.mainMaterialDescriptorSetLayout = CreateDescriptorSetLayout(m_device, static_cast<uint32_t>(
        materialDescriptorSetLayoutBindings.size()), materialDescriptorSetLayoutBindings.data());
        //Add both descriptor set layout to the array that will be passed to the pipeline layout function
        std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = 
        {
            m_globalSceneDescriptorSetLayout, m_mainMaterialData.mainMaterialDescriptorSetLayout
        };
        //Setting the push constant that will be passed to the shader for every object to specify their model matrix
        VkPushConstantRange pushConstants{};
        CreatePushConstantRange(pushConstants, VK_SHADER_STAGE_VERTEX_BIT, sizeof(DrawDataPushConstant));
        //Create the pipeline layout with all descriptor sets and push constants
        CreatePipelineLayout(m_device, opaquePipelineBuilder.m_pLayout, static_cast<uint32_t>(descriptorSetLayouts.size()), 
        descriptorSetLayouts.data(), 1, &pushConstants);

        //build the pipeline
        opaquePipelineBuilder.Build();

        //Since indirect draw will have to use a different shader, a new pipeline needs to be created for it
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            opaquePipelineBuilder.m_pGraphicsPipeline = &(m_mainMaterialData.indirectDrawOpaqueGraphicsPipeline);
            opaquePipelineBuilder.m_pLayout = &(m_mainMaterialData.globalIndirectDrawPipelineLayout);

            opaquePipelineBuilder.CreateShaderStage("VulkanShaders/IndirectDrawShader.vert.glsl.spv", VK_SHADER_STAGE_VERTEX_BIT, "main");
            opaquePipelineBuilder.CreateShaderStage("VulkanShaders/MainGeometryShader.frag.glsl.spv", VK_SHADER_STAGE_FRAGMENT_BIT, "main");
            opaquePipelineBuilder.SetTriangleListInputAssembly();
            opaquePipelineBuilder.SetDynamicViewport();
            opaquePipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
            opaquePipelineBuilder.SetPrimitiveCulling(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
            opaquePipelineBuilder.DisableDepthBias();
            opaquePipelineBuilder.DisableDepthClamp();
            opaquePipelineBuilder.DisableMultisampling();
            opaquePipelineBuilder.EnableDefaultDepthTest();
            opaquePipelineBuilder.DisableColorBlending();

            //The indirect pipeline does not use push constants since there will be very few draw calls each frame
            CreatePipelineLayout(m_device, opaquePipelineBuilder.m_pLayout,static_cast<uint32_t>(descriptorSetLayouts.size()), 
            descriptorSetLayouts.data(), 0, nullptr);

            opaquePipelineBuilder.Build();
        #endif
    }

    void VulkanRenderer::UploadGlobalBuffersToGPU(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, 
    std::vector<MaterialConstants>& materialConstants, std::vector<DrawIndirectData>& drawIndirectCommands)
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

        //Allocates the buffer that will hold all the material constant data of the objects that have been loaded
        VkDeviceSize materialBufferSize = static_cast<VkDeviceSize>(materialConstants.size() * sizeof(MaterialConstants));
        AllocateBuffer(m_globalMaterialConstantsBuffer, materialBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
        //Store the address of the material buffer to the scene data so that it can be passed to the shaders every frame
        VkBufferDeviceAddressInfo materialBufferAddressInfo{};
        materialBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        materialBufferAddressInfo.buffer = m_globalMaterialConstantsBuffer.buffer;
        m_globalSceneData.materialConstantsBufferAddress = vkGetBufferDeviceAddress(m_device, &materialBufferAddressInfo);

        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            //Allocate the buffer that will hold the indirect commands 
            VkDeviceSize indirectBufferSize = sizeof(DrawIndirectData) * drawIndirectCommands.size();
            AllocateBuffer(m_drawIndirectDataBuffer, indirectBufferSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 
            VMA_MEMORY_USAGE_GPU_ONLY);
            //Store the device address of the indirect buffer, so that it can be passed to the shaders every frame
            VkBufferDeviceAddressInfo indirectBufferAddressInfo{};
            indirectBufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            indirectBufferAddressInfo.buffer = m_drawIndirectDataBuffer.buffer;
            m_globalSceneData.indirectBufferAddress = vkGetBufferDeviceAddress(m_device, &indirectBufferAddressInfo);

            //Allocate the buffer that will hold the data used for frustum culling
            VkDeviceSize frustumCullingDataBufferSize = sizeof(FrustumCollisionData) * m_mainDrawContext.surfaceFrustumCollisions.size();
            AllocateBuffer(m_surfaceFrustumCollisionBuffer, frustumCullingDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
            //Store the device address of the frustum culling buffer, so that it can be passed to the shaders every frame
            VkBufferDeviceAddressInfo frustumDataBufferDeviceAddressInfo{};
            frustumDataBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            frustumDataBufferDeviceAddressInfo.buffer = m_surfaceFrustumCollisionBuffer.buffer;
            m_globalSceneData.frustumCollisionBufferAddress = vkGetBufferDeviceAddress(m_device, &frustumDataBufferDeviceAddressInfo);
        #endif

        //Allocate a staging buffer to access the buffer data and copy it to the GPU buffers
        AllocatedBuffer stagingBuffer;
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            AllocateBuffer(stagingBuffer, vertexBufferSize + indexBufferSize + materialBufferSize + indirectBufferSize + frustumCullingDataBufferSize, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        #else
            AllocateBuffer(stagingBuffer, vertexBufferSize + indexBufferSize + materialBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);
        #endif
        void* allBuffersData = stagingBuffer.allocation->GetMappedData();
        //Copy all the necessary data to the staging buffer at the right offsets
        memcpy(allBuffersData, vertices.data(), static_cast<size_t>(vertexBufferSize));
        memcpy(reinterpret_cast<char*>(allBuffersData) + static_cast<size_t>(vertexBufferSize), indices.data(), 
        static_cast<size_t>(indexBufferSize));
        memcpy(reinterpret_cast<char*>(allBuffersData) + static_cast<size_t>(vertexBufferSize + indexBufferSize), materialConstants.data(),
        static_cast<size_t>(materialBufferSize));
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            memcpy(reinterpret_cast<char*>(allBuffersData) + static_cast<size_t>(vertexBufferSize + indexBufferSize + materialBufferSize), 
            drawIndirectCommands.data(), static_cast<size_t>(indirectBufferSize));
            memcpy(reinterpret_cast<char*>(allBuffersData) + static_cast<size_t>(vertexBufferSize + indexBufferSize + materialBufferSize + 
            indirectBufferSize), m_mainDrawContext.surfaceFrustumCollisions.data(), static_cast<size_t>(frustumCullingDataBufferSize));
        #endif

        //Start recording commands for copying the staging buffer into the GPU buffers
        StartRecordingCommands();

        //Specify the parts of the staging buffer that will be copied and into which parts of the vertex buffer
        VkBufferCopy2 vertexBufferCopyRegion{};
        vertexBufferCopyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        vertexBufferCopyRegion.size = vertexBufferSize;
        vertexBufferCopyRegion.srcOffset = 0;
        vertexBufferCopyRegion.dstOffset = 0;
        //Specify the two source and destination buffer and the region struct from above
        VkCopyBufferInfo2 vertexBufferCopy{};
        vertexBufferCopy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        vertexBufferCopy.srcBuffer = stagingBuffer.buffer;
        vertexBufferCopy.dstBuffer = m_globalIndexAndVertexBuffer.vertexBuffer.buffer;
        vertexBufferCopy.regionCount = 1;
        vertexBufferCopy.pRegions = &vertexBufferCopyRegion;
        //Record the copy command
        vkCmdCopyBuffer2(m_commands.commandBuffer, &vertexBufferCopy);

        //Specify the parts of the staging buffer that will be copied and into which parts of the index buffer
        VkBufferCopy2 indexBufferCopyRegion{};
        indexBufferCopyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        indexBufferCopyRegion.size = indexBufferSize;
        indexBufferCopyRegion.srcOffset = vertexBufferSize;
        indexBufferCopyRegion.dstOffset = 0;
        //Specify the two source and destination buffer and the region struct from above
        VkCopyBufferInfo2 indexBufferCopy{};
        indexBufferCopy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        indexBufferCopy.srcBuffer = stagingBuffer.buffer;
        indexBufferCopy.dstBuffer = m_globalIndexAndVertexBuffer.indexBuffer.buffer;
        indexBufferCopy.regionCount = 1;
        indexBufferCopy.pRegions = &indexBufferCopyRegion;
        //Record the copy command
        vkCmdCopyBuffer2(m_commands.commandBuffer, &indexBufferCopy);

        //Specify the parts of the staging buffer that will be copied and into which parts of the material buffer
        VkBufferCopy2 materialConstantsBufferCopyRegion{};
        materialConstantsBufferCopyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
        materialConstantsBufferCopyRegion.size = materialBufferSize;
        materialConstantsBufferCopyRegion.srcOffset = vertexBufferSize + indexBufferSize;
        materialConstantsBufferCopyRegion.dstOffset = 0;
        //Specify the two source and destination buffer and the region struct from above
        VkCopyBufferInfo2 materialConstantsBufferCopy{};
        materialConstantsBufferCopy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
        materialConstantsBufferCopy.srcBuffer = stagingBuffer.buffer;
        materialConstantsBufferCopy.dstBuffer = m_globalMaterialConstantsBuffer.buffer;
        materialConstantsBufferCopy.regionCount = 1;
        materialConstantsBufferCopy.pRegions = &materialConstantsBufferCopyRegion;
        //Record the copy command
        vkCmdCopyBuffer2(m_commands.commandBuffer, &materialConstantsBufferCopy);

        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            //Specify the parts of the staging buffer that will be copied and into which parts of the indirect buffer
            VkBufferCopy2 indirectBufferCopyRegion{};
            indirectBufferCopyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
            indirectBufferCopyRegion.size = indirectBufferSize;
            indirectBufferCopyRegion.srcOffset = vertexBufferSize + indexBufferSize + materialBufferSize; 
            indirectBufferCopyRegion.dstOffset = 0;
            //Specify the two source and destination buffer and the region struct from above
            VkCopyBufferInfo2 indirectBufferCopy{};
            indirectBufferCopy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
            indirectBufferCopy.srcBuffer = stagingBuffer.buffer;
            indirectBufferCopy.dstBuffer = m_drawIndirectDataBuffer.buffer;
            indirectBufferCopy.regionCount = 1;
            indirectBufferCopy.pRegions = &indirectBufferCopyRegion;
            //Record the copy command
            vkCmdCopyBuffer2(m_commands.commandBuffer, &indirectBufferCopy);

            //Specify the parts of the staging buffer that will be copied and into which parts of the frustum collision buffer
            VkBufferCopy2 frustumCollisionBufferCopyRegion{};
            frustumCollisionBufferCopyRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
            frustumCollisionBufferCopyRegion.size = frustumCullingDataBufferSize;
            frustumCollisionBufferCopyRegion.srcOffset = vertexBufferSize + indexBufferSize + materialBufferSize + indirectBufferSize; 
            frustumCollisionBufferCopyRegion.dstOffset = 0;
            //Specify the two source and destination buffer and the region struct from above
            VkCopyBufferInfo2 furstumCollisionBufferCopy{};
            furstumCollisionBufferCopy.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
            furstumCollisionBufferCopy.srcBuffer = stagingBuffer.buffer;
            furstumCollisionBufferCopy.dstBuffer = m_surfaceFrustumCollisionBuffer.buffer;
            furstumCollisionBufferCopy.regionCount = 1;
            furstumCollisionBufferCopy.pRegions = &frustumCollisionBufferCopyRegion;
            //Record the copy command
            vkCmdCopyBuffer2(m_commands.commandBuffer, &furstumCollisionBufferCopy);
        #endif

        SubmitCommands();

        vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.allocation);
    }

    void VulkanRenderer::WriteMaterialData(MaterialInstance& materialInstance, MaterialPass pass)
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
    }

    void VulkanRenderer::UploadMaterialResourcesToGPU(std::vector<MaterialResources>& materialResources)
    {
        //Allocate the descriptor set that will be used for material resource access
        VkDescriptorPoolSize materialDescriptorPoolSize{};
        materialDescriptorPoolSize.descriptorCount = 2;
        materialDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; 
        m_mainMaterialData.descriptorAllocator.Init(&m_device, 1, &materialDescriptorPoolSize);
        m_mainMaterialData.descriptorAllocator.AllocateDescriptorSet(&(m_mainMaterialData.mainMaterialDescriptorSetLayout), 1, 
        &m_mainMaterialData.mainMaterialDescriptorSet);

        std::vector<VkDescriptorImageInfo> baseColorTextureImageInfos(materialResources.size());
        for(size_t i = 0; i < materialResources.size(); ++i)
        {
            baseColorTextureImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            baseColorTextureImageInfos[i].imageView = materialResources[i].colorImage.imageView;
            baseColorTextureImageInfos[i].sampler = materialResources[i].colorSampler;
        }
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.descriptorCount = static_cast<uint32_t>(materialResources.size());
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstSet = m_mainMaterialData.mainMaterialDescriptorSet;
        descriptorWrite.pImageInfo = baseColorTextureImageInfos.data();
        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
    }

    void VulkanRenderer::LoadScene(std::string& filepath, const char* sceneName, std::vector<Vertex>& vertices, 
    std::vector<uint32_t>& indices, std::vector<MaterialConstants>& materialConstants, std::vector<MaterialResources>& materialResources)
    {
        BLIT_INFO("Loading GLTF: %s", filepath.c_str())

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
            BLIT_ERROR("Failed to find filepath, GLTF loading abandoned")
            return;
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
                BLIT_ERROR("Failed to load glTF: %i", fastgltf::to_underlying(load.error()))
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
                BLIT_ERROR("Failed to load glTF: %i", fastgltf::to_underlying(load.error()))
                return;
            }
        } 
        else 
        {
            BLIT_ERROR("Failed to determine glTF container")
            return;
        }


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

        size_t previousMaterialsSize = materialConstants.size();
        materialConstants.resize(materialConstants.size() + gltf.materials.size());
        materialResources.resize(materialResources.size() + gltf.materials.size());
        materials.resize(gltf.materials.size());
        /*Load materials*/
        for(size_t i = 0; i < gltf.materials.size(); ++i)
        {
            //Add a new entry in the materials of the scene and add a pointer to it in the placeholder array
            scene.m_materials[gltf.materials[i].name.c_str()] = MaterialInstance();
            materials[i] = &(scene.m_materials[gltf.materials[i].name.c_str()]);
            //Storing the index that will be passed to the shader to access the data of this material
            materials[i]->materialIndex = static_cast<uint32_t>(previousMaterialsSize + i);

            //Get the base color data of the material
            materialConstants[i + previousMaterialsSize].colorFactors.x = gltf.materials[i].pbrData.baseColorFactor[0];
            materialConstants[i + previousMaterialsSize].colorFactors.y = gltf.materials[i].pbrData.baseColorFactor[1];
            materialConstants[i + previousMaterialsSize].colorFactors.z = gltf.materials[i].pbrData.baseColorFactor[2];
            materialConstants[i + previousMaterialsSize].colorFactors.w = gltf.materials[i].pbrData.baseColorFactor[3];
            //Get the metallic and rough factors of the material
            materialConstants[i + previousMaterialsSize].metalRoughFactors.x = gltf.materials[i].pbrData.metallicFactor;
            materialConstants[i + previousMaterialsSize].metalRoughFactors.y = gltf.materials[i].pbrData.roughnessFactor;

            //Simple distinction between transparent and opaque objects, not doing anything with transparent materials for now
            MaterialPass passType = MaterialPass::Opaque;
            if (gltf.materials[i].alphaMode == fastgltf::AlphaMode::Blend)
            {
                passType = MaterialPass::Transparent;
            }
       
            //Load placeholder resources for the material
            materialResources[i + previousMaterialsSize].colorImage = m_placeholderGreyTexture;
            materialResources[i + previousMaterialsSize].colorSampler = m_placeholderLinearSampler;
            materialResources[i + previousMaterialsSize].metalRoughImage = m_placeholderGreyTexture;
            materialResources[i + previousMaterialsSize].metalRoughSampler = m_placeholderLinearSampler;

            if(gltf.materials[i].pbrData.baseColorTexture.has_value())
            {
                //Get the index of the texture used by this material
                size_t materialColorImageIndex = gltf.textures[gltf.materials[i].pbrData.baseColorTexture.value().textureIndex].
                imageIndex.value();
                size_t materialColorSamplerIndex = gltf.textures[gltf.materials[i].pbrData.baseColorTexture.value().textureIndex].
                samplerIndex.value();

                materialResources[i + previousMaterialsSize].colorImage = *textureImages[materialColorImageIndex];
                materialResources[i + previousMaterialsSize].colorSampler = scene.m_samplers[materialColorSamplerIndex];
            }

            //This is an old function but I am keeping it because it gives each material their pipeline and I don't want to handle that now
            WriteMaterialData(scene.m_materials[gltf.materials[i].name.c_str()], passType);
        }

        //Start iterating through all the meshes that were loaded from gltf
        meshAssets.resize(gltf.meshes.size());
        for(size_t i = 0; i < gltf.meshes.size(); ++i)
        {
            //Add a new mesh to the vulkan mesh assets array and save its name
            scene.m_meshes[gltf.meshes[i].name.c_str()] = MeshAsset();
            meshAssets[i] = &(scene.m_meshes[gltf.meshes[i].name.c_str()]);
            MeshAsset* pMesh = meshAssets[i];
            pMesh->assetName = gltf.meshes[i].name;
            pMesh->surfaces.resize(gltf.meshes[i].primitives.size());
            size_t surfaceIndex = 0;
            //Iterate through all the surfaces in the mesh asset
            for (auto& primitive : gltf.meshes[i].primitives)
            {
                //Add a new geoSurface object at the back of the mesh assets array and update its indices
                GeoSurface& currentSurface = pMesh->surfaces[surfaceIndex];
                currentSurface.firstIndex = static_cast<uint32_t>(indices.size());
                currentSurface.indexCount = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count);

                //The first vertex of this surface is the size of the vertices array before it start loading this surface's vertices
                size_t initialVertex = vertices.size();

                /* Load indices */
                fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor, [&](std::uint32_t idx) {
                    indices.push_back(idx + static_cast<uint32_t>(initialVertex));
                });

                /* Load vertex positions */
                fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->second];
                size_t previousVerticesSize = vertices.size();
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](glm::vec3 v, size_t index) {
                    Vertex& newVertex = vertices[initialVertex + index];
                    newVertex.position = v;
                    newVertex.normal = { 1, 0, 0 };
                    newVertex.color = glm::vec4{ 1.f };
                    newVertex.uvMapX = 0;
                    newVertex.uvMapY = 0;
                });

                //Derive the oriented bounding box of the surface
                glm::vec3 minPos = vertices[initialVertex].position; 
                glm::vec3 maxPos = vertices[initialVertex].position;
                for(size_t i = initialVertex; i < vertices.size(); ++i)
                {
                    minPos = glm::min(minPos, vertices[initialVertex].position);
                    maxPos = glm::max(maxPos, vertices[initialVertex].position);
                }
                currentSurface.obb.origin = (minPos + maxPos) * 0.5f;
                currentSurface.obb.extents = (maxPos - minPos) * 0.5f;
                //Using the oriented bounding box the bounding sphere can also be derived
                currentSurface.sphereCollision.center.x = glm::distance(maxPos.x, minPos.x) /** 0.5f*/;
                currentSurface.sphereCollision.center.y = glm::distance(maxPos.y, minPos.y) /** 0.5f*/;
                currentSurface.sphereCollision.center.z = glm::distance(maxPos.z, minPos.z) /** 0.5f*/;
                float radius = glm::length(currentSurface.obb.extents);

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
                    pMesh->surfaces[surfaceIndex].pMaterial = materials[primitive.materialIndex.value()];
                } 
                else 
                {
                    pMesh->surfaces[surfaceIndex].pMaterial = materials[0];
                }
                ++surfaceIndex;
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

            //Takes a variant (node.transform) and calls the correct function to derive the local transform of each mesh
            std::visit(
            fastgltf::visitor 
            { 
                [&](fastgltf::Node::TransformMatrix matrix) {
                    memcpy(&(nodes.back()->localTransform), matrix.data(), sizeof(matrix));
                },
                [&](fastgltf::Node::TRS transform) {
                    //Get the translation vector to create the translation matrix
                    glm::vec3 translation(transform.translation[0], transform.translation[1], transform.translation[2]);
                    //Create a quaternion to create the rotation matrix of the transform
                    glm::quat rotation(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                    //Get the scale vector to create the scale matrix
                    glm::vec3 scale(transform.scale[0], transform.scale[1], transform.scale[2]);
                    //Create the matrices
                    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), translation);
                    glm::mat4 rotationMatrix = glm::toMat4(rotation);
                    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), scale);
                    //Derive the local transform
                    nodes.back()->localTransform = translationMatrix * rotationMatrix * scaleMatrix;
                 } 
            }
            , node.transform);

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
    void VulkanRenderer::DrawFrame(RenderContext& context)
    {
        // No way to know this for now, since I removed GLFW
        if(context.bResize)
        {
            BootstrapRecreateSwapchain();
        }

        BlitzenEngine::Camera* pCamera = reinterpret_cast<BlitzenEngine::Camera*>(context.pCamera);

        /*-------------------------------
        Setting up the global scene data
        ---------------------------------*/
        //Setup the projection and view matrix
        m_globalSceneData.viewMatrix = pCamera->GetViewMatrix();
        m_globalSceneData.projectionMatrix = pCamera->GetProjectionMatrix();
        //Invert the projection matrix so that it matches glm and objects are not drawn upside down
        m_globalSceneData.projectionMatrix[1][1] *= -1;
        m_globalSceneData.projectionViewMatrix = m_globalSceneData.projectionMatrix * m_globalSceneData.viewMatrix;


        //Default lighting parameters
        m_globalSceneData.ambientColor = glm::vec4(1.f);
        m_globalSceneData.sunlightColor = glm::vec4(1.f);
        m_globalSceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
        //Give the address of the shared vertex buffer to the global scene data
        m_globalSceneData.vertexBufferAddress = m_globalIndexAndVertexBuffer.vertexBufferAddress;
        /*------------------------------
        Global scene data set
        ------------------------------*/


        /*
        Initializing frustum culling data
        */
        glm::vec4 frustumData[6];
        frustumData[0] = m_globalSceneData.viewMatrix[3] + m_globalSceneData.viewMatrix[0];
        frustumData[1] = m_globalSceneData.viewMatrix[3] - m_globalSceneData.viewMatrix[0];
        frustumData[2] = m_globalSceneData.viewMatrix[3] + m_globalSceneData.viewMatrix[1];
        frustumData[3] = m_globalSceneData.viewMatrix[3] - m_globalSceneData.viewMatrix[1];
        frustumData[4] = m_globalSceneData.viewMatrix[3] - m_globalSceneData.viewMatrix[2];
        frustumData[5] = glm::vec4(0, 0, -1, 10000.f);
        //If indirect mode is not active frustum culling is done on the cpu
        if(!stats.drawIndirectMode)
        {
            for(RenderObject& object : m_mainDrawContext.opaqueRenderObjects)
            {
                bool visible = true;
                for(size_t i = 0; i < 6; ++i)
                {
                    glm::vec3 center = object.modelMatrix * glm::vec4((object.sphereCollision.center), 1.f); 
                    float radius = object.sphereCollision.radius * object.scale;
                    visible = visible && (glm::dot(frustumData[i], glm::vec4(center, 1)) > -radius);
                }
                object.bVisible = visible;
            }
        }

        //Getting the tools that will be used this frame
        VkCommandBuffer& frameCommandBuffer = m_frameTools[m_currentFrame].graphicsCommandBuffer;
        VkFence& currentFrameCompleteFence = m_frameTools[m_currentFrame].frameCompleteFence;
        VkSemaphore& currentImageAcquiredSemaphore = m_frameTools[m_currentFrame].imageAcquiredSemaphore;
        VkSemaphore& currentReadyToPresentSemaphore = m_frameTools[m_currentFrame].readyToPresentSemaphore;
        DescriptorAllocator& frameDescriptorAllocator = m_frameTools[m_currentFrame].sceneDataDescriptroAllocator;
        //#ifndef NDEBUG
            VkQueryPool& currentTimestampQueryPool = m_frameTools[m_currentFrame].timestampQueryPool;
        //#endif

        //Wait for queue submition to signal the fence with a timeout of 1 second
        vkWaitForFences(m_device, 1, &currentFrameCompleteFence, VK_TRUE, 1000000000);
        vkResetFences(m_device, 1, &currentFrameCompleteFence);


        //Read the timestamp of the previous frame
        //#ifndef NDEBUG
            uint64_t queryResults[2];
            vkGetQueryPoolResults(m_device, m_frameTools[m_currentFrame].timestampQueryPool, 
            0, 2, sizeof(queryResults), queryResults, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(m_bootstrapObjects.chosenGPU, &props);
            double gpuStart = static_cast<double>(queryResults[0]) * props.limits.timestampPeriod * 1e-6;
            double gpuEnd = static_cast<double>(queryResults[1]) * props.limits.timestampPeriod * 1e-6;
            double frameGPU = gpuEnd - gpuStart;
            char title[256];
            sprintf_s(title, "GPU: %lf", frameGPU);
        //#endif


        //Passing global scene data to the shaders
        m_frameTools[m_currentFrame].sceneDataDescriptroAllocator.ResetDescriptorPools();
        //Write the new data in the scene data struct to the buffer's memory
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
        //VK_CHECK(vkAcquireNextImageKHR(m_device, m_bootstrapObjects.swapchain, 1000000000, 
        //m_frameTools[m_currentFrame].imageAcquiredSemaphore,VK_NULL_HANDLE, &swapchainImageIndex));

        VkResult rese = vkAcquireNextImageKHR(m_device, m_bootstrapObjects.swapchain, 1000000000, m_frameTools[m_currentFrame].imageAcquiredSemaphore,
            VK_NULL_HANDLE, &swapchainImageIndex);

        //Reset the timestamp before recording commands
        //#ifndef NDEBUG
            vkResetQueryPool(m_device, currentTimestampQueryPool, 0, 2);
        //#endif
        //Return the command buffer to the initial state and then put it in the recording state
        vkResetCommandBuffer(frameCommandBuffer, 0);
        VkCommandBufferBeginInfo commandBufferBeginInfo{};
        commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(frameCommandBuffer, &commandBufferBeginInfo);

        /*--------------------------------------------------------------------------------------------
        After this point and until vkEndCommandBuffer is called, all graphics commands that need to be 
        called during the current frame will be recorded
        --------------------------------------------------------------------------------------------*/

        /*-------------------------------------------------------------------------------------------------------------------------- 
        This block of code is responsible for the color of the window (at some point this should be managed by a compute shader)
        ----------------------------------------------------------------------------------------------------------------------------*/
        TransitionImageLayout(frameCommandBuffer, m_drawingAttachment.image, 
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
        vkCmdClearColorImage(frameCommandBuffer, m_drawingAttachment.image, VK_IMAGE_LAYOUT_GENERAL,
        &clearColorValue, 1, &clearColorImageSubresourceRange);
        /*---------------------------------------------------
        Command recording for image clearing complete
        -----------------------------------------------------*/

        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            vkCmdBindPipeline(frameCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_indirectCullingComputePipelineData.pipeline);
            vkCmdDispatch(frameCommandBuffer, (static_cast<uint32_t>(m_mainDrawContext.indirectDrawData.size()) + 31) / 32, 1, 1);
        #endif


        //After clearing the image, transition color attachments and any other attachments so that they can be used for rendering
        TransitionImageLayout(frameCommandBuffer, m_drawingAttachment.image, VK_IMAGE_LAYOUT_GENERAL, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        TransitionImageLayout(frameCommandBuffer, m_depthAttachment.image, VK_IMAGE_LAYOUT_UNDEFINED, 
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);


        /*-----------------------------------------------------
        This block of code sets up the rendering attachments
        ------------------------------------------------------*/
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
        /*---------------------------------------------------------------
        Attachments ready to being rendering
        -------------------------------------------------------------*/


        /*--------------------------------------------------------------------------------------------
        After this point and until vkCmdEndRendring is called, all rendering commands are recorded
        ---------------------------------------------------------------------------------------------*/
        vkCmdBeginRendering(frameCommandBuffer, &mainRenderingInfo); 

        //Bind the index buffer, it's the same for both the indirect and traditional version of the pipeline
        vkCmdBindIndexBuffer(frameCommandBuffer, m_globalIndexAndVertexBuffer.indexBuffer.buffer, 
        0, VK_INDEX_TYPE_UINT32);
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            //Bind the pipeline and the global descriptor set for the indirect pipeline if it is active
            if(stats.drawIndirectMode)
            {
                vkCmdBindPipeline(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                m_mainMaterialData.indirectDrawOpaqueGraphicsPipeline);
                VkDescriptorSet descriptorSetsToBind [2] = {sceneDataDescriptorSet, m_mainMaterialData.mainMaterialDescriptorSet};
                vkCmdBindDescriptorSets(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                m_mainMaterialData.globalIndirectDrawPipelineLayout, 0, 2, descriptorSetsToBind, 0, nullptr);
            }
            //Bind the pipeline and the global descriptor set for the traditional pipeline, if indirect is inactive
            else
            {
                vkCmdBindPipeline(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                m_mainMaterialData.opaquePipeline.graphicsPipeline);
                VkDescriptorSet descriptorSetsToBind [2] = {sceneDataDescriptorSet, m_mainMaterialData.mainMaterialDescriptorSet};
                vkCmdBindDescriptorSets(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                m_mainMaterialData.opaquePipeline.pipelineLayout, 0, 2, descriptorSetsToBind, 0, nullptr);
            }
        #else
            vkCmdBindPipeline(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_mainMaterialData.opaquePipeline.graphicsPipeline);
            VkDescriptorSet descriptorSetsToBind[2] = { sceneDataDescriptorSet, m_mainMaterialData.mainMaterialDescriptorSet };
            vkCmdBindDescriptorSets(frameCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_mainMaterialData.opaquePipeline.pipelineLayout, 0, 2, descriptorSetsToBind, 0, nullptr);
        #endif
        //Since viewport and scissor were specified in the dynamic state, they now need to be set
        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = static_cast<float>(m_drawExtent.width);
        viewport.height = static_cast<float>(m_drawExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(frameCommandBuffer, 0, 1, &viewport);
        VkRect2D scissor{};
        scissor.extent.width = m_drawExtent.width;
        scissor.extent.height = m_drawExtent.height;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        vkCmdSetScissor(frameCommandBuffer, 0, 1, &scissor);

        //Start a timestamp 
        //#ifndef NDEBUG
            vkCmdWriteTimestamp2(frameCommandBuffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, currentTimestampQueryPool, 0);
        //#endif 
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            if(stats.drawIndirectMode)
            {
                //This is just beatiful
                vkCmdDrawIndexedIndirect(frameCommandBuffer, m_drawIndirectDataBuffer.buffer, offsetof(DrawIndirectData, indirectDraws), 
                static_cast<uint32_t>(m_mainDrawContext.indirectDrawData.size()), sizeof(DrawIndirectData));
            }
            //Go with the traditional method if draw indirect is inactive
            else
            {
                for (const RenderObject& opaque : m_mainDrawContext.opaqueRenderObjects)
                {
                    if(opaque.bVisible)
                    {
                        DrawDataPushConstant pushConstant;
                        pushConstant.modelMatrix = opaque.modelMatrix;
                        pushConstant.materialIndex = opaque.pMaterial->materialIndex;
                        vkCmdPushConstants(frameCommandBuffer, opaque.pMaterial->pPipeline->pipelineLayout,
                            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DrawDataPushConstant), &pushConstant);
                        vkCmdDrawIndexed(frameCommandBuffer, opaque.indexCount, 1, opaque.firstIndex, 0, 0);
                    }
                }
            }
        #else
            for (const RenderObject& opaque : m_mainDrawContext.opaqueRenderObjects)
            {
                if(opaque.bVisible)
                {
                    DrawDataPushConstant pushConstant;
                    pushConstant.modelMatrix = opaque.modelMatrix;
                    pushConstant.materialIndex = opaque.pMaterial->materialIndex;
                    vkCmdPushConstants(frameCommandBuffer, opaque.pMaterial->pPipeline->pipelineLayout,
                        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DrawDataPushConstant), &pushConstant);
                    vkCmdDrawIndexed(frameCommandBuffer, opaque.indexCount, 1, opaque.firstIndex, 0, 0);
                }
            }
        #endif
        //End the timestamp when drawing commands end
        //#ifndef NDEBUG
            vkCmdWriteTimestamp2(frameCommandBuffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, currentTimestampQueryPool, 1);
        //#endif
        /*-------------------------------
        End of rendering commands
        ---------------------------------*/
        vkCmdEndRendering(frameCommandBuffer);


        //After the color attachment is done being used for rendering, it transition for a data transfer to the swapchain image, which will be presented to the surface
        TransitionImageLayout(frameCommandBuffer, m_drawingAttachment.image, 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, 
        VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT);
        TransitionImageLayout(frameCommandBuffer, m_bootstrapObjects.swapchainImages[swapchainImageIndex], 
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, 
        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT);
        CopyImageToImage(frameCommandBuffer, m_drawingAttachment.image, 
        m_bootstrapObjects.swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        m_drawExtent, m_bootstrapObjects.swapchainExtent);
        //This final layout of the swapchain is the one that can present to screen
        TransitionImageLayout(frameCommandBuffer, m_bootstrapObjects.swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR , VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT, 
        VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE);
        /*------------------------------------------------------------------------------------------------------------------------------
        All commands end here, and the command buffer will be put to the executable state and will be submitted to the graphics queue, 
        so that the graphics commands of this frame can be executed
        --------------------------------------------------------------------------------------------------------------------------------*/
        vkEndCommandBuffer(frameCommandBuffer);


        /*------------------------------------------------------------------------------------------------------
        This code block submits the command buffer and configures the synchronization for command excecution
        ---------------------------------------------------------------------------------------------------------*/
        //Initializing the info for the semaphore that tells commands to stop until a swapchain image is acquired
        VkSemaphoreSubmitInfo waitSemaphoreInfo{};
        waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSemaphoreInfo.deviceIndex = 0;
        waitSemaphoreInfo.value = 1;
        waitSemaphoreInfo.semaphore = currentImageAcquiredSemaphore;
        waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        //Initializing the info for the semaphore that tells commands that rendering commands are done and the frame is ready for presentation
        VkSemaphoreSubmitInfo signalSemaphoreInfo{};
        signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSemaphoreInfo.deviceIndex = 0;
        signalSemaphoreInfo.value = 1;
        signalSemaphoreInfo.semaphore = currentReadyToPresentSemaphore;
        signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        //Initializing the info for the graphics command buffer that will be submitted
        VkCommandBufferSubmitInfo graphicsCommandBufferInfo{};
        graphicsCommandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        graphicsCommandBufferInfo.commandBuffer = frameCommandBuffer;
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
        vkQueueSubmit2(m_queues.graphicsQueue, 1, &graphicsCommandsSubmit, currentFrameCompleteFence);
        /*------------------------------------------------------
        Graphics command buffer submitted to graphics queue
        --------------------------------------------------------*/


        //Finally set up the presentation, so that rendering result appear on screen
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &currentReadyToPresentSemaphore;
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

        CreateSwapchain(m_pWindowWidth, m_pWindowHeight);

        //The draw extent should also be updated depending on if the swapchain got bigger or smaller
        m_drawExtent.width = std::min(static_cast<uint32_t>(*m_pWindowWidth), 
        static_cast<uint32_t>(m_drawingAttachment.extent.width));
        m_drawExtent.height = std::min(static_cast<uint32_t>(*m_pWindowHeight), 
        static_cast<uint32_t>(m_drawingAttachment.extent.height));
    }
}