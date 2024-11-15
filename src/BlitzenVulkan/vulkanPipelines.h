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
        void CreateShaderStage(const char* filepath, VkShaderStageFlagBits shaderStage, const char* entryPointerName);

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

        void CreatePipelineLayout(uint32_t descriptorSetLayoutCount, VkDescriptorSetLayout* pDescriptorSetLayouts, uint32_t pushConstantRangeCount, 
        VkPushConstantRange* pPushConstantRanges);

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
}