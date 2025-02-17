#pragma once

#include "vulkanData.h"

namespace BlitzenVulkan
{
    class GraphicsPipelineBuilder
    {
    public:

        //Called to initialize the device, pipeline and layout pointers (unnecessary since those variables are public)
        void Init(const VkDevice* pDevice, VkPipelineLayout* pLayout, VkPipeline* pPipeline);

        //Once all graphics pipeline settings have been configured, this can be called to build the pipeline
        void Build();

        //Reads the shader code found in a file and assigns it to the specified shader stage
        void CreateShaderStage(const char* filepath, VkShaderStageFlagBits shaderStage, const char* entryPointName);

        void SetTriangleListInputAssembly();

        void SetDynamicViewport();

        void SetPolygonMode(VkPolygonMode polygonMode);
        void SetPrimitiveCulling(VkCullModeFlags cullMode, VkFrontFace frontFace);
        void DisableDepthBias();
        void DisableDepthClamp();
        
        void DisableMultisampling();

        void DisableDepthTest();
        void EnableDefaultDepthTest();

        void DisableColorBlending();

    private:

        //Called at the end of build, so that a specific instance of graphics pipeline build can be reused if necessary
        void ResetResources();

    public:

        //A pointer to the pipeline layout that is to be initialized by the builder
        VkPipelineLayout* m_pLayout = nullptr;

        //A pointer to the graphics pipeline that is to be initialized by the builder
        VkPipeline* m_pGraphicsPipeline = nullptr;

        //Since this is an external class, it's going to need a pointer to the renderer's device to build the layout and pipeline
        const VkDevice* m_pDevice = nullptr;

    private:

        //Shader code must be wrapped in a VkShaderModule struct to be passed to the shader stages
        std::vector<std::vector<char>> m_shaderCodes;
        std::vector<VkShaderModule> m_shaderModules{};
        std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages{};
        //A valid vertex input state needs to be passed to vkGraphicsPipelineCreateInfo, even though the renderer's pipelines will not be using it
        VkPipelineVertexInputStateCreateInfo m_vertexInput{};

        VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};
        VkPipelineTessellationStateCreateInfo m_tessellation{};
        VkPipelineViewportStateCreateInfo m_viewport{};
        VkPipelineRasterizationStateCreateInfo m_rasterization{};
        VkPipelineMultisampleStateCreateInfo m_multisample{};
        VkPipelineDepthStencilStateCreateInfo m_depthStencil{};

        VkPipelineColorBlendAttachmentState m_colorBlendAttachment{};
        VkPipelineColorBlendStateCreateInfo m_colorBlending{};

        std::vector<VkDynamicState> m_dynamicStates{};
        VkPipelineDynamicStateCreateInfo m_dynamicState{};
    };

    //Helper function that compiles spir-v and adds it to shader stage. Will aid in the creation of graphics and compute pipelines
    void CreateShaderProgram(const VkDevice& device, const char* filepath, VkShaderStageFlagBits shaderStage, const char* entryPointName, 
    VkShaderModule& shaderModule, VkPipelineShaderStageCreateInfo& pipelineShaderStage, std::vector<char>& shaderCode);

    //Helper function that creates a pipeline layout that will be used by a compute or graphics pipeline
    void CreatePipelineLayout(VkDevice device, VkPipelineLayout* layout, uint32_t descriptorSetLayoutCount, 
    VkDescriptorSetLayout* pDescriptorSetLayouts, uint32_t pushConstantRangeCount, VkPushConstantRange* pPushConstantRanges);

    //Helper function for pipeline layout creation, takes care of a single set layout binding creation
    void CreateDescriptorSetLayoutBinding(VkDescriptorSetLayoutBinding& bindingInfo, uint32_t binding, uint32_t descriptorCount, 
    VkDescriptorType descriptorType, VkShaderStageFlags shaderStage, VkSampler* pImmutableSamplers = nullptr);

    //Helper function for pipeline layout creation, takes care of a single descriptor set layout creation
    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, uint32_t bindingCount, VkDescriptorSetLayoutBinding* pBindings);

    //Helper function for pipeline layout creation, takes care of a single push constant creation
    void CreatePushConstantRange(VkPushConstantRange& pushConstant, VkShaderStageFlags shaderStage, uint32_t size, uint32_t offset = 0);

    //Holds all the relevant data for a compute pipeline
    struct ComputePipelineData
    {
        VkPipeline pipeline;
        VkPipelineLayout layout;
        std::vector<VkDescriptorSetLayout> setLayouts;
    };
}