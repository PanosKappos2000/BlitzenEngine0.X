#include "vulkanPipelines.h"

namespace BlitzenVulkan
{
    void GraphicsPipelineBuilder::Init(const VkDevice* pDevice, VkPipelineLayout* pLayout, VkPipeline* pPipeline)
    {
        m_pDevice = pDevice; 
        m_pLayout = pLayout;
        m_pGraphicsPipeline = pPipeline;
    }

    void GraphicsPipelineBuilder::CreateShaderStage(const char* filepath, VkShaderStageFlagBits shaderStage, 
    const char* entryPointName)
    {
        m_shaderCodes.push_back(std::vector<char>());
        //Initialize a file reader for the .spv in binary format and with the cursor at the end of the file
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        //If the file did not open, something might be wrong with the filepath, so it needs to be checked
        if (!file.is_open())
        {
        	__debugbreak();
        }
        size_t filesize = static_cast<size_t>(file.tellg());
        m_shaderCodes.back().resize(filesize);
        //put the file cursor at the beginning
        file.seekg(0);
        // load the entire file into the array
        file.read(m_shaderCodes.back().data(), filesize);
        file.close();

        //Wrap the code in a shader module object
        VkShaderModuleCreateInfo shaderModuleInfo{};
        shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleInfo.codeSize = static_cast<uint32_t>(m_shaderCodes.back().size());
        shaderModuleInfo.pCode = reinterpret_cast<uint32_t*>(m_shaderCodes.back().data());
        m_shaderModules.push_back(VkShaderModule());
        vkCreateShaderModule(*m_pDevice, &shaderModuleInfo, nullptr, &m_shaderModules.back());

        //Adds a new shader stage based on that shader module
        m_shaderStages.push_back(VkPipelineShaderStageCreateInfo{});
        m_shaderStages.back().sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_shaderStages.back().module = m_shaderModules.back();
        m_shaderStages.back().stage = shaderStage;
        m_shaderStages.back().pName = entryPointName;
    }

    void GraphicsPipelineBuilder::SetTriangleListInputAssembly()
    {
        m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        m_inputAssembly.primitiveRestartEnable = VK_FALSE;
    }

    void GraphicsPipelineBuilder::SetDynamicViewport()
    {
        m_viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_viewport.scissorCount = 1;
        m_viewport.viewportCount = 1;

        //Add scissor and viewport to the the dynamic state, so that they can be add to VkDynamicStateCreateInfo
        m_dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
        m_dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
    }

    void GraphicsPipelineBuilder::SetPolygonMode(VkPolygonMode polygonMode)
    {
        //Setting the default values for the rasterizer that all pipelines will use
        m_rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        m_rasterization.rasterizerDiscardEnable = VK_FALSE;
        m_rasterization.lineWidth = 1.0f;

        m_rasterization.polygonMode = polygonMode;
    }

    void GraphicsPipelineBuilder::SetPrimitiveCulling(VkCullModeFlags cullMode, VkFrontFace frontFace)
    {
        m_rasterization.cullMode = cullMode;
        m_rasterization.frontFace = frontFace;
    }

    void GraphicsPipelineBuilder::DisableDepthBias()
    {
        m_rasterization.depthBiasEnable = VK_FALSE;
    }

    void GraphicsPipelineBuilder::DisableDepthClamp()
    {
        m_rasterization.depthClampEnable = VK_FALSE;
    }

    void GraphicsPipelineBuilder::DisableMultisampling()
    {
        m_multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_multisample.sampleShadingEnable = VK_FALSE;
        m_multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        m_multisample.minSampleShading = 1.0f;
        m_multisample.pSampleMask = nullptr;
        m_multisample.alphaToCoverageEnable = VK_FALSE;
        m_multisample.alphaToOneEnable = VK_FALSE;
    }

    void GraphicsPipelineBuilder::EnableDefaultDepthTest()
    {
        m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_depthStencil.depthTestEnable = VK_TRUE;
        m_depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        m_depthStencil.depthWriteEnable = VK_TRUE;
        m_depthStencil.stencilTestEnable = VK_FALSE;
        m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    }

    void GraphicsPipelineBuilder::DisableDepthTest()
    {
        m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        m_depthStencil.depthTestEnable = VK_FALSE;
        m_depthStencil.depthWriteEnable = VK_FALSE;
        m_depthStencil.stencilTestEnable = VK_FALSE;
        m_depthStencil.depthBoundsTestEnable = VK_FALSE;
    }

    void GraphicsPipelineBuilder::DisableColorBlending()
    {
        m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        m_colorBlendAttachment.blendEnable = VK_FALSE;

        m_colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        m_colorBlending.logicOpEnable = VK_FALSE;
        m_colorBlending.attachmentCount = 1;
        m_colorBlending.pAttachments = &m_colorBlendAttachment;
    }

    void GraphicsPipelineBuilder::CreatePipelineLayout(uint32_t descriptorSetLayoutCount, VkDescriptorSetLayout* pDescriptorSetLayouts, 
    uint32_t pushConstantRangeCount, VkPushConstantRange* pPushConstantRanges)
    {
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = descriptorSetLayoutCount;
        layoutInfo.pSetLayouts = pDescriptorSetLayouts;
        layoutInfo.pushConstantRangeCount = pushConstantRangeCount;
        layoutInfo.pPushConstantRanges = pPushConstantRanges;
        vkCreatePipelineLayout(*m_pDevice, &layoutInfo, nullptr, m_pLayout);
    }

    void GraphicsPipelineBuilder::Build()
    {
        //Before building the grapics pipelines, all specified dynamic states are added to the dynamic state info
        m_dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        m_dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
        m_dynamicState.pDynamicStates = m_dynamicStates.data();

        VkGraphicsPipelineCreateInfo graphicsPipelineInfo{};
        graphicsPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphicsPipelineInfo.layout = *m_pLayout;
        graphicsPipelineInfo.renderPass = VK_NULL_HANDLE;
        
        graphicsPipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
        graphicsPipelineInfo.pStages = m_shaderStages.data();

        //Passing a placeholder vertex input
        m_vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        graphicsPipelineInfo.pVertexInputState = &m_vertexInput;

        graphicsPipelineInfo.pInputAssemblyState = &m_inputAssembly;

        graphicsPipelineInfo.pTessellationState = &m_tessellation;

        graphicsPipelineInfo.pRasterizationState = &m_rasterization;

        graphicsPipelineInfo.pViewportState = &m_viewport;

        graphicsPipelineInfo.pDynamicState = &m_dynamicState;

        graphicsPipelineInfo.pMultisampleState = &m_multisample;

        graphicsPipelineInfo.pDepthStencilState = &m_depthStencil;

        graphicsPipelineInfo.pColorBlendState = &m_colorBlending;

        //All pipelines will use the dynamic rendering model so it gets set here
        VkPipelineRenderingCreateInfo pipelineDynamicRendering{};
        pipelineDynamicRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        //The attachments are fixed for now for all pipelines. There will be one color attachment and one depth attachments
        pipelineDynamicRendering.colorAttachmentCount = 1;
        VkFormat colorAttachmentFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        pipelineDynamicRendering.pColorAttachmentFormats = &colorAttachmentFormat;
        pipelineDynamicRendering.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        //Not using a stencil attachment for now
        pipelineDynamicRendering.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
        graphicsPipelineInfo.pNext = &pipelineDynamicRendering;

        vkCreateGraphicsPipelines(*m_pDevice, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, m_pGraphicsPipeline);

        ResetResources();
    }

    void GraphicsPipelineBuilder::ResetResources()
    {
        for(size_t i = 0; i < m_shaderModules.size(); ++i)
        {
            vkDestroyShaderModule(*m_pDevice, m_shaderModules[i], nullptr);
        }
        m_shaderCodes.clear();
        m_shaderModules.clear();
        m_shaderStages.clear();
        m_inputAssembly = {};
        m_tessellation = {};
        m_viewport = {};
        m_rasterization = {};
        m_multisample = {};
        m_depthStencil = {};

        m_colorBlendAttachment = {};
        m_colorBlending = {};

        m_dynamicStates.clear();
        m_dynamicState = {};

        m_pGraphicsPipeline = nullptr;
        m_pLayout = nullptr;
    }
}