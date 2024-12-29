#include "vulkanRenderer.h"
#include "Platform/blitPlatform.h"

namespace BlitzenVulkan
{
    /*
        These function are used load the function pointer for creating the debug messenger
    */
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) 
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) 
        {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else 
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) 
        {
            func(instance, debugMessenger, pAllocator);
        }
    }
    // Debug messenger callback function
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) 
    {
        std::cout << "validation error: " << pCallbackData->pMessage << '\n';
        return VK_FALSE;
    }
    // Validation layers function pointers


    void VulkanRenderer::Init(void* pState, uint32_t* pWidth, uint32_t* pHeight)
    {
        m_pWindowWidth = pWidth;
        m_pWindowHeight = pHeight; 

        /*
            If the renderer ever uses a custom allocator it should be initialized here
        */
        m_pCustomAllocator = nullptr;

        /*------------------------
            VkInstance Creation
        -------------------------*/
        {
            //Will be passed to the VkInstanceCreateInfo that will create Vulkan's instance
            VkApplicationInfo applicationInfo{};
            applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            applicationInfo.pNext = nullptr; // Not using this
            applicationInfo.apiVersion = VK_API_VERSION_1_3; // There are some features and extensions Blitzen will use, that exist in Vulkan 1.3
            // These are not important right now but passing them either way, to be thorough
            applicationInfo.pApplicationName = BLITZEN_VULKAN_USER_APPLICATION;
            applicationInfo.applicationVersion = BLITZEN_VULKAN_USER_APPLICATION_VERSION;
            applicationInfo.pEngineName = BLITZEN_VULKAN_USER_ENGINE;
            applicationInfo.engineVersion = BLITZEN_VULKAN_USER_ENGINE_VERSION;

            VkInstanceCreateInfo instanceInfo {};
            instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instanceInfo.pNext = nullptr; // Will be used if validation layers are activated later in this function
            instanceInfo.flags = 0; // Not using this

            // TODO: Use what is stored in the dynamic arrays below to check if all extensions are supported
            uint32_t extensionsCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(static_cast<size_t>(extensionsCount));
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, availableExtensions.data());

            // Creating an array of required extension names to pass to ppEnabledExtensionNames
            const char* requiredExtensionNames [BLITZEN_VULKAN_ENABLED_EXTENSION_COUNT];
            requiredExtensionNames[0] =  VULKAN_SURFACE_KHR_EXTENSION_NAME;
            requiredExtensionNames[1] = "VK_KHR_surface";        
            instanceInfo.enabledExtensionCount = BLITZEN_VULKAN_ENABLED_EXTENSION_COUNT;
            instanceInfo.enabledLayerCount = 0;

            //If this is a debug build, the validation layer extension is also needed
            #ifndef NDEBUG

                requiredExtensionNames[2] = "VK_EXT_debug_utils";

                // Getting all supported validation layers
                uint32_t availableLayerCount = 0;
                vkEnumerateInstanceLayerProperties(&availableLayerCount, nullptr);
                std::vector<VkLayerProperties> availableLayers(static_cast<size_t>(availableLayerCount));
                vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers.data());

                // Checking if the requested validation layers are supported
                uint8_t layersFound = 0;
                for(size_t i = 0; i < availableLayers.size(); i++)
                {
                   if(strcmp(availableLayers[i].layerName,VALIDATION_LAYER_NAME))
                   {
                       layersFound = 1;
                       break;
                   }
                }

                BLIT_ASSERT_MESSAGE(layersFound, "The vulkan renderer will not be used in debug mode without validation layers")

                // If the above check is passed validation layers can be safely loaded
                instanceInfo.enabledLayerCount = 1;
                const char* layerNameRef = VALIDATION_LAYER_NAME;
                instanceInfo.ppEnabledLayerNames = &layerNameRef;

                VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
                debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debugMessengerInfo.pfnUserCallback = debugCallback;
                debugMessengerInfo.pNext = nullptr;// Not using this right now
                debugMessengerInfo.pUserData = nullptr; // Not using this right now

                // The debug messenger needs to be referenced by the instance
                instanceInfo.pNext = &debugMessengerInfo;
            #endif

            instanceInfo.ppEnabledExtensionNames = requiredExtensionNames;
             
            instanceInfo.pApplicationInfo = &applicationInfo;

            // Create the instance
            VK_CHECK(vkCreateInstance(&instanceInfo, m_pCustomAllocator, &(m_bootstrapObjects.instance)))
        }




        #ifndef NDEBUG
        /*---------------------------------
            Debug messenger creation 
        ----------------------------------*/
        {
            // This is the same as the one loaded to VkInstanceCreateInfo
            VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo{};
            debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugMessengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugMessengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugMessengerInfo.pfnUserCallback = debugCallback;
            debugMessengerInfo.pNext = nullptr;// Not using this right now
            debugMessengerInfo.pUserData = nullptr; // Not using this right now

            VK_CHECK(CreateDebugUtilsMessengerEXT(m_bootstrapObjects.instance, &debugMessengerInfo, m_pCustomAllocator, 
                &(m_bootstrapObjects.debugMessenger)))
        }
        /*--------------------------------------------------------
            Debug messenger created and stored in m_initHandles
        ----------------------------------------------------------*/
        #endif



        {
            BlitzenPlatform::PlatformState* pTrueState = reinterpret_cast<BlitzenPlatform::PlatformState*>(pState);
            BlitzenPlatform::CreateVulkanSurface(pTrueState, m_bootstrapObjects.instance, m_bootstrapObjects.surface, m_pCustomAllocator);
        }

        /*
            Physical device (GPU representation) selection
        */
        {
            uint32_t physicalDeviceCount = 0;
            vkEnumeratePhysicalDevices(m_bootstrapObjects.instance, &physicalDeviceCount, nullptr);
            //BLIT_ASSERT(physicalDeviceCount)
            std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
            vkEnumeratePhysicalDevices(m_bootstrapObjects.instance, &physicalDeviceCount, physicalDevices.data());

            // Goes through the available devices, to eliminate the ones that are completely inadequate
            for(size_t i = 0; i < physicalDevices.size(); ++i)
            {
                VkPhysicalDevice& pdv = physicalDevices[i];

                //Retrieve queue families from device
                uint32_t queueFamilyPropertyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties2(pdv, &queueFamilyPropertyCount, nullptr);
                // Remove this device from the candidates, if no queue families were retrieved
                if(!queueFamilyPropertyCount)
                {
                    //TODO: Fix this later
                    //physicalDevices.erase(i);
                    //--i;
                    continue;
                }
                // Store the queue family properties to query for their indices
                std::vector<VkQueueFamilyProperties2> queueFamilyProperties(static_cast<size_t>(queueFamilyPropertyCount));
                for(size_t i = 0; i < queueFamilyProperties.size(); ++i)
                {
                    queueFamilyProperties[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
                }
                vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevices[i], &queueFamilyPropertyCount, queueFamilyProperties.data());
                for(size_t i = 0; i < queueFamilyProperties.size(); ++i)
                {
                    // Checks for a graphics queue index, if one has not already been found 
                    if(queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && !m_queues.hasGraphicsIndex)
                    {
                        m_queues.graphicsIndex = i;
                        m_queues.hasGraphicsIndex = true;
                    }

                    // Checks for a compute queue index, if one has not already been found 
                    if(queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT && !m_queues.hasComputeIndex)
                    {
                        m_queues.computeIndex = i;
                        m_queues.hasComputeIndex = true;
                    }

                    VkBool32 supportsPresent = VK_FALSE;
                    vkGetPhysicalDeviceSurfaceSupportKHR(pdv, i, m_bootstrapObjects.surface, &supportsPresent);
                    if(supportsPresent == VK_TRUE && !m_queues.hasPresentIndex)
                    {
                        m_queues.presentIndex = i;
                        m_queues.hasPresentIndex = true;
                    }
                }

                // If one of the required queue families has no index, then it gets removed from the candidates
                /*if(!m_presentQueue.hasIndex || !m_graphicsQueue.hasIndex || !m_computeQueue.hasIndex)
                {
                    physicalDevices.RemoveAtIndex(i);
                    --i;
                }*///TODO: Add this functionality
            }

            for(size_t i = 0; i < physicalDevices.size(); ++i)
            {
                VkPhysicalDevice& pdv = physicalDevices[i];

                //Retrieve queue families from device
                VkPhysicalDeviceProperties2 props{};
                props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                vkGetPhysicalDeviceProperties2(pdv, &props);

                // Prefer discrete gpu if there is one
                if(props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    m_bootstrapObjects.chosenGPU = pdv;
                    stats.hasDiscreteGPU = true;
                }
            }

            // If a discrete GPU is not found, the renderer chooses the 1st device. This will change the way the renderer goes forward
            if(!stats.hasDiscreteGPU)
                m_bootstrapObjects.chosenGPU = physicalDevices[0];
        }

        /*-----------------------
            Device creation
        -------------------------*/
        {
            VkDeviceCreateInfo deviceInfo{};
            deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceInfo.flags = 0; // Not using this
            deviceInfo.enabledLayerCount = 0;//Deprecated

            // Only using the swapchain extension for now
            deviceInfo.enabledExtensionCount = 1;
            const char* extensionsNames = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            deviceInfo.ppEnabledExtensionNames = &extensionsNames;

            // Standard device features
            VkPhysicalDeviceFeatures deviceFeatures{};
            #if BLITZEN_START_VULKAN_WITH_INDIRECT
                deviceFeatures.multiDrawIndirect = true;
            #endif
            deviceInfo.pEnabledFeatures = nullptr; // The device features will be added VkDeviceFeatures2

            // Extended device features
            VkPhysicalDeviceVulkan11Features vulkan11Features{};
            vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            #if BLITZEN_START_VULKAN_WITH_INDIRECT
                vulkan11Features.shaderDrawParameters = true;
            #endif

            VkPhysicalDeviceVulkan12Features vulkan12Features{};
            vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            vulkan12Features.bufferDeviceAddress = true;
            vulkan12Features.descriptorIndexing = true;
            //Allows vulkan to reset queries
            vulkan12Features.hostQueryReset = true;
            //Allows spir-v shaders to use descriptor arrays
            vulkan12Features.runtimeDescriptorArray = true;

            VkPhysicalDeviceVulkan13Features vulkan13Features{};
            vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            //Using dynamic rendering since the engine will not benefit from the VkRenderPass
            vulkan13Features.dynamicRendering = true;
            vulkan13Features.synchronization2 = true;

            VkPhysicalDeviceFeatures2 vulkanExtendedFeatures{};
            vulkanExtendedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            vulkanExtendedFeatures.features = deviceFeatures;

            deviceInfo.pNext = &vulkanExtendedFeatures;
            vulkanExtendedFeatures.pNext = &vulkan11Features;
            vulkan11Features.pNext = &vulkan12Features;
            vulkan12Features.pNext = &vulkan13Features;

            std::vector<VkDeviceQueueCreateInfo> queueInfos(1);
            queueInfos[0].queueFamilyIndex = m_queues.graphicsIndex;
            // If compute has a different index from present, add a new info for it
            if(m_queues.graphicsIndex != m_queues.computeIndex)
            {
                queueInfos.emplace_back(VkDeviceQueueCreateInfo());
                queueInfos[1].queueFamilyIndex = m_queues.computeIndex;
            }
            // If an info was created for compute and present is not equal to compute or graphics, create a new one for present as well
            if(queueInfos.size() == 2 && queueInfos[0].queueFamilyIndex != m_queues.presentIndex && queueInfos[1].queueFamilyIndex != m_queues.presentIndex)
            {
                queueInfos.emplace_back(VkDeviceQueueCreateInfo());
                queueInfos[2].queueFamilyIndex = m_queues.presentIndex;
            }
            // If an info was not created for compute but present has a different index from the other 2, create a new info for it
            if(queueInfos.size() == 1 && queueInfos[0].queueFamilyIndex != m_queues.presentIndex)
            {
                queueInfos.push_back(VkDeviceQueueCreateInfo());
                queueInfos[1].queueFamilyIndex = m_queues.presentIndex;
            }
            // With the count of the queue infos found and the indices passed, the rest is standard
            float priority = 1.f;
            for(size_t i = 0; i < queueInfos.size(); ++i)
            {
                queueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueInfos[i].pNext = nullptr; // Not using this
                queueInfos[i].flags = 0; // Not using this
                queueInfos[i].queueCount = 1;
                queueInfos[i].pQueuePriorities = &priority;
            }
            // Pass the queue infos
            deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
            deviceInfo.pQueueCreateInfos = queueInfos.data();

            // Create the device
            VK_CHECK(vkCreateDevice(m_bootstrapObjects.chosenGPU, &deviceInfo, m_pCustomAllocator, &m_device))

            // Retrieve graphics queue handle
            VkDeviceQueueInfo2 graphicsQueueInfo{};
            graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
            graphicsQueueInfo.pNext = nullptr; // Not using this
            graphicsQueueInfo.flags = 0; // Not using this
            graphicsQueueInfo.queueFamilyIndex = m_queues.graphicsIndex;
            graphicsQueueInfo.queueIndex = 0;
            vkGetDeviceQueue2(m_device, &graphicsQueueInfo, &m_queues.graphicsQueue);

            // Retrieve compute queue handle
            VkDeviceQueueInfo2 computeQueueInfo{};
            computeQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
            computeQueueInfo.pNext = nullptr; // Not using this
            computeQueueInfo.flags = 0; // Not using this
            computeQueueInfo.queueFamilyIndex = m_queues.computeIndex;
            computeQueueInfo.queueIndex = 0;
            vkGetDeviceQueue2(m_device, &computeQueueInfo, &m_queues.computeQueue);

            // Retrieve present queue handle
            VkDeviceQueueInfo2 presentQueueInfo{};
            presentQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
            presentQueueInfo.pNext = nullptr; // Not using this
            presentQueueInfo.flags = 0; // Not using this
            presentQueueInfo.queueFamilyIndex = m_queues.presentIndex;
            presentQueueInfo.queueIndex = 0;
            vkGetDeviceQueue2(m_device, &presentQueueInfo, &m_queues.presentQueue);
        }
        /*
            Device created and saved to m_device. Queue handles retrieved
        */


        CreateSwapchain(pWidth, pHeight);
        
        
        InitAllocator();
        InitCommands();
    }

    void VulkanRenderer::CreateSwapchain(uint32_t* pWidth, uint32_t* pHeight)
    {
        m_bootstrapObjects.swapchainExtent = {*pWidth, *pHeight};

        // This will be needed to find some details about swapchain support
        VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo{};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
        surfaceInfo.surface = m_bootstrapObjects.surface;
        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.pNext = nullptr;
        swapchainInfo.flags = 0;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.clipped = VK_TRUE;// Don't present things renderer out of bounds
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchainInfo.surface = m_bootstrapObjects.surface;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
        /* Image format and color space */
        {
            // Retrieve surface format to check for desired swapchain format and color space
            uint32_t surfaceFormatsCount = 0; 
            vkGetPhysicalDeviceSurfaceFormatsKHR(m_bootstrapObjects.chosenGPU, m_bootstrapObjects.surface, &surfaceFormatsCount, nullptr);
            std::vector<VkSurfaceFormatKHR> surfaceFormats(static_cast<size_t>(surfaceFormatsCount));
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_bootstrapObjects.chosenGPU, 
            m_bootstrapObjects.surface, &surfaceFormatsCount, surfaceFormats.data()))
            // Look for the desired image format
            uint8_t found = 0;
            for(size_t i = 0; i < surfaceFormats.size(); ++i)
            {
                // If the desire image format is found assign it to the swapchain info and break out of the loop
                if(surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && 
                surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
                    swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                    // Save the format to init handles
                    m_bootstrapObjects.swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
                    found = 1;
                    break;
                }
            }
            // If the desired format is not found (unlikely), assign the first one that is supported and hope for the best ( I'm just a chill guy )
            if(!found)
            {
                swapchainInfo.imageFormat = surfaceFormats[0].format;
                // Save the image format
                m_bootstrapObjects.swapchainImageFormat = swapchainInfo.imageFormat;
                swapchainInfo.imageColorSpace = surfaceFormats[0].colorSpace;
            }
        }
        /* Present mode */
        {
            // Retrieve the presentation modes supported, to look for the desired one
            uint32_t presentModeCount = 0;
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_bootstrapObjects.chosenGPU, m_bootstrapObjects.surface, &presentModeCount, nullptr));
            std::vector<VkPresentModeKHR> presentModes(static_cast<size_t>(presentModeCount));
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_bootstrapObjects.chosenGPU, m_bootstrapObjects.surface, &presentModeCount, 
            presentModes.data()))
            // Look for the desired presentation mode
            uint8_t found = 0;
            for(size_t i = 0; i < presentModes.size(); ++i)
            {
                // If the desired presentation mode is found, set the swapchain info to that
                if(presentModes[i] == DESIRED_SWAPCHAIN_PRESENTATION_MODE)
                {
                    swapchainInfo.presentMode = DESIRED_SWAPCHAIN_PRESENTATION_MODE;
                    found = 1;
                    break;
                }
            }
            
            // If it was not found, set it to this random smuck ( Don't worry, I'm a proffesional )
            if(!found)
            {
                swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
        }
        //Set the swapchain extent to the window's width and height
        m_bootstrapObjects.swapchainExtent = {static_cast<uint32_t>(*m_pWindowWidth), static_cast<uint32_t>(*m_pWindowHeight)};
        // Retrieve surface capabilities to properly configure some swapchain values
        VkSurfaceCapabilitiesKHR surfaceCapabilities{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_bootstrapObjects.chosenGPU, m_bootstrapObjects.surface, &surfaceCapabilities));
        /* Swapchain extent */
        {
            // Get the swapchain extent from the surface capabilities, if it is not some weird value
            if(surfaceCapabilities.currentExtent.width != UINT32_MAX)
            {
                m_bootstrapObjects.swapchainExtent = surfaceCapabilities.currentExtent;
            }
            // Get the min extent and max extent allowed by the GPU,  to clamp the initial value
            VkExtent2D minExtent = surfaceCapabilities.minImageExtent;
            VkExtent2D maxExtent = surfaceCapabilities.maxImageExtent;
            m_bootstrapObjects.swapchainExtent.width = std::clamp(m_bootstrapObjects.swapchainExtent.width, maxExtent.width, minExtent.width);
            m_bootstrapObjects.swapchainExtent.height = std::clamp(m_bootstrapObjects.swapchainExtent.height, maxExtent.height, minExtent.height);
            // Swapchain extent fully checked and ready to pass to the swapchain info
            swapchainInfo.imageExtent = m_bootstrapObjects.swapchainExtent;
        }
        /* Min image count */
        {
            uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
            // Check that image count did not supass max image count
            if(surfaceCapabilities.maxImageCount > 0 && surfaceCapabilities.maxImageCount < imageCount )
            {
                imageCount = surfaceCapabilities.maxImageCount;
            }
            // Swapchain image count fully check and ready to be pass to the swapchain info
            swapchainInfo.minImageCount = imageCount;
        }
        swapchainInfo.preTransform = surfaceCapabilities.currentTransform;
        if (m_queues.graphicsIndex != m_queues.presentIndex)
        {
            uint32_t queueFamilyIndices[] = {m_queues.graphicsIndex, m_queues.presentIndex};
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchainInfo.queueFamilyIndexCount = 2;
            swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
        } 
        else 
        {
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchainInfo.queueFamilyIndexCount = 0;// Unnecessary if the indices are the same
        }
        // Create the swapchain
        VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainInfo, m_pCustomAllocator, &m_bootstrapObjects.swapchain));
        // Retrieve the swapchain images
        uint32_t swapchainImageCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_bootstrapObjects.swapchain, &swapchainImageCount, nullptr))
        m_bootstrapObjects.swapchainImages.resize(swapchainImageCount);
        VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_bootstrapObjects.swapchain, &swapchainImageCount, m_bootstrapObjects.swapchainImages.data()))
        // Create image view for each swapchain image
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
            vmaDestroyBuffer(m_allocator, m_finalIndirectBuffer.buffer, m_finalIndirectBuffer.allocation);
            vmaDestroyBuffer(m_allocator, m_surfaceFrustumCollisionBuffer.buffer, m_surfaceFrustumCollisionBuffer.allocation);
            vkDestroyPipeline(m_device, m_indirectCullingComputePipelineData.pipeline, nullptr);
            vkDestroyPipelineLayout(m_device, m_indirectCullingComputePipelineData.layout, nullptr);
            for(size_t i = 0; i < m_indirectCullingComputePipelineData.setLayouts.size(); ++i)
            {
                vkDestroyDescriptorSetLayout(m_device, m_indirectCullingComputePipelineData.setLayouts[i], nullptr);
            }
        #endif
        vmaDestroyBuffer(m_allocator, m_globalMaterialConstantsBuffer.buffer, m_globalMaterialConstantsBuffer.allocation);
        vmaDestroyBuffer(m_allocator, m_globalIndexAndVertexBuffer.vertexBuffer.buffer, 
        m_globalIndexAndVertexBuffer.vertexBuffer.allocation);
        vmaDestroyBuffer(m_allocator, m_globalIndexAndVertexBuffer.indexBuffer.buffer, 
        m_globalIndexAndVertexBuffer.indexBuffer.allocation);

        m_mainMaterialData.CleanupResources(m_device);

        //The scene data descriptor set layout has already been destroyed if Blitzen started with indirect
        #if !BLITZEN_START_VULKAN_WITH_INDIRECT
            vkDestroyDescriptorSetLayout(m_device, m_globalSceneDescriptorSetLayout, nullptr);
        #endif

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
        descriptorAllocator.CleanupResources();
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

            //#ifndef NDEBUG
                vkDestroyQueryPool(m_device, m_frameTools[i].timestampQueryPool, nullptr);
            //#endif

            m_frameTools[i].sceneDataDescriptroAllocator.CleanupResources();
            vmaDestroyBuffer(m_allocator, m_frameTools[i].sceneDataUniformBuffer.buffer, 
            m_frameTools[i].sceneDataUniformBuffer.allocation);

            #if BLITZEN_START_VULKAN_WITH_INDIRECT
                vmaDestroyBuffer(m_allocator, m_frameTools[i].indirectFrustumDataUniformBuffer.buffer, 
            m_frameTools[i].indirectFrustumDataUniformBuffer.allocation);
            #endif
        }
    }

    void VulkanRenderer::CleanupBootstrapInitializedObjects()
    {
        vkDestroySwapchainKHR(m_device, m_bootstrapObjects.swapchain, nullptr);

        vkDestroyDevice(m_device, nullptr);

        vkDestroySurfaceKHR(m_bootstrapObjects.instance, m_bootstrapObjects.surface, nullptr);
        DestroyDebugUtilsMessengerEXT(m_bootstrapObjects.instance, m_bootstrapObjects.debugMessenger, m_pCustomAllocator);
        vkDestroyInstance(m_bootstrapObjects.instance, nullptr);
    }
}