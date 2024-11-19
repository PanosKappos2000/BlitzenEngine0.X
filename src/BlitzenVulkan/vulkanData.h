#pragma once

#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <fstream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vma/vk_mem_alloc.h"

//Checks if Blitzen starts with draw indirect available
#define BLITZEN_START_VULKAN_WITH_INDIRECT 1

namespace BlitzenVulkan
{
    class VulkanRenderer;

    //This struct will automatically handle all descriptor allocations
    class DescriptorAllocator
    {
    public:
        //Better suited when the size and amount of descriptor sets that will be allocated is not known
        void Init(const VkDevice* pDV);
        //Better suited when the size and amount of descriptor sets is known from the start
        void Init(const VkDevice* pDV, uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes);

        void AllocateDescriptorSet(VkDescriptorSetLayout* pLayouts, uint32_t setCount, VkDescriptorSet* pSet);
        //If a descriptor set gets reallocated every frame, this should be used to reset its pool
        void ResetDescriptorPools();

        void CleanupResources();
    private:
        std::vector<VkDescriptorPool> m_fullPools;
        std::vector<VkDescriptorPool> m_availablePools;

        //Saves the pool sizes on initialization, so that subsequent allocation create the same size descriptor pools
        uint32_t m_poolSizeCount;
        VkDescriptorPoolSize* m_pPoolSizes;

        const VkDevice* m_pDevice;

        void CreateDescriptorPool(uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes);
        VkDescriptorPool GetDescriptorPool();
    };

    //All images the renderer will be allocated using vma and will depend on this struct
    struct AllocatedImage
    {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;

        VkExtent3D extent;
        VkFormat format;

        void CleanupResources(const VkDevice& device, const VmaAllocator& allocator);
    };

    struct AllocatedBuffer
    {
        VkBuffer buffer;
        VmaAllocation allocation;
        VmaAllocationInfo allocationInfo;
    };

    //Represents a vertex processed in shaders. UvMaps split into 2 floats, for alignment purposes
    struct Vertex 
    {
        glm::vec3 position;
        float uvMapX;
        glm::vec3 normal;
        float uvMapY;
        glm::vec4 color;
    };

    struct alignas(16)SceneData
    {
        glm::vec4 sunlightColor;
        glm::vec4 sunlightDirection;
        glm::vec4 ambientColor;
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::mat4 projectionViewMatrix;
        //The scene data will be used to pass the vertex buffer address, this will probably need to be changed later
        VkDeviceAddress vertexBufferAddress; 
        //The scene data will be used to pass the material constants buffer address, this will probably need to be changed later
        VkDeviceAddress materialConstantsBufferAddress;
        //The scene data will be used to pass the indirect draw buffer address, this will probably need to be changed later
        VkDeviceAddress indirectBufferAddress;
    };

    struct MaterialPipeline
    {
        VkPipeline graphicsPipeline;
        VkPipelineLayout pipelineLayout;
    };

    enum class MaterialPass : uint8_t
    {
        Transparent,
        Opaque,
        Undefined
    };  

    struct MaterialInstance
    {
        //Used to index into the array of material constants that will be passed in the shaders as a storage buffer
        uint32_t materialIndex;
        //Used to bind the pipeline that will be used by a specific material
        MaterialPipeline* pPipeline;
        MaterialPass pass;
    };

    //Used to represent each surface of a mesh. The firstIndex and indexCount are used for vkCmdDrawIndexed
    struct GeoSurface
    {
        uint32_t firstIndex;
        uint32_t indexCount;

        MaterialInstance* pMaterial;
    };

    struct MeshAsset
    {
        std::vector<GeoSurface> surfaces;
        std::string assetName;
    };

    struct ModelMatrixPushConstant
    {
        glm::mat4 modelMatrix;
    };

    struct MeshBuffers
    {
        AllocatedBuffer vertexBuffer;
        AllocatedBuffer indexBuffer;
        VkDeviceAddress vertexBufferAddress;
    };

    struct alignas(16) MaterialConstants
    {
        //How lighting should affect normal textures
	    glm::vec4 colorFactors;
        //How lighting should affect textures with special metallic properties
	    glm::vec4 metalRoughFactors;
    };

    struct MaterialResources
    {
        //Holds the normal color part of the texture
	    AllocatedImage colorImage;
        //Sampler for the above texture
	    VkSampler colorSampler;
        //Holds the parts of the texture with special metallic properties like shininess
	    AllocatedImage metalRoughImage;
        //Sampler for the above texture
	    VkSampler metalRoughSampler;
        //The data buffer where all uniform buffers descriptor will be written
	    VkBuffer dataBuffer;
        //The offset for this specific resource
	    uint32_t dataBufferOffset;
    };

    //Holds material data, for the different types of materials that will be used by the renderer
    struct GlobalMaterialData
    {
        MaterialPipeline opaquePipeline;
        MaterialPipeline transparentPipeline;

        VkDescriptorSetLayout mainMaterialDescriptorSetLayout;

        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            VkPipeline indirectDrawOpaqueGraphicsPipeline;
            VkPipelineLayout globalIndirectDrawPipelineLayout;
        #endif

        void CleanupResources(const VkDevice& device);
    };

    struct RenderObject
    {
        //Used to call vkCmdDrawIndexed
        uint32_t firstIndex;
        uint32_t indexCount;

        //Used to bind the correct pipeline, descriptor set and pipeline layout 
        MaterialInstance* pMaterial;

        //This specifies the model matrix with which
        glm::mat4 modelMatrix;
    };

    //Holds the commands for a specific draw call with multi draw indirect and also some per draw data
    struct alignas(16) DrawIndirectData
    {
        //World matrix will be indexed inside the shader with gl_DrawIDARB
        glm::mat4 worldMatrix;
        //Material index will be indexed inside the shader with gl_DrawIDARB and will return the material constant data
        uint32_t materialIndex;

        VkDrawIndexedIndirectCommand indirectDraws;
    };

    struct DrawContext
    {
        std::vector<RenderObject> opaqueRenderObjects;

        //If Blitzen uses indirect commands, the draw context will include an array of draw indirect data
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            std::vector<DrawIndirectData> indirectDrawData;
        #endif
    };

    struct VulkanStats
    {
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            bool drawIndirectMode = true;
        #endif
    };

    class Node
    {
    public:
        Node* pParent;
        std::vector<Node*> m_children;

        //Each node starts with a local transform which is then updated into the world transform by updating with a parent matrix
        glm::mat4 worldTransform;
        glm::mat4 localTransform;

        //Some Nodes also include a mesh asset, that makes them mesh nodes and AddToDrawContext has extra functionality for them
        MeshAsset* pMesh = nullptr;
    public:
        void AddToDrawContext(glm::mat4& topMatrix, DrawContext& drawContext);

        //Takes a top matrix and uses it to initialize the world transform
        void UpdateTransform(const glm::mat4& topMatrix);
    };

    class LoadedScene
    {
    public:
        std::unordered_map<std::string, Node> m_nodes;
        std::unordered_map<std::string, MeshAsset> m_meshes;
        std::unordered_map<std::string, AllocatedImage> m_textures;
        std::unordered_map<std::string, MaterialInstance> m_materials;

        //Holds all pure parent nodes(ones that have no parent nodes)
        std::vector<Node*> m_pureParentNodes;

        //Holds all the samplers used for the textures in a scene
        std::vector<VkSampler> m_samplers;

        //Used to allocate all material descriptor sets in the scene
        DescriptorAllocator m_descriptorAllocator;

        //Holds the uniform buffer used for material constants
        AllocatedBuffer m_materialDataBuffer;

        VulkanRenderer* m_pRenderer;

        void AddToDrawContext(glm::mat4& topMatrix, DrawContext& drawContex);

        void ClearAll();
    };
}