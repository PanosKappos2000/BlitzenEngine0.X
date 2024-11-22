#pragma once

#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <fstream>

#include "Core/math.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vma/vk_mem_alloc.h"

//Checks if Blitzen starts with draw indirect available
#define BLITZEN_START_VULKAN_WITH_INDIRECT 1
#define BLITZEN_FRUSTUM_CULLING_BOUNDING_SPHERE 0

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

    struct OrientedBoundingBox
    {
        glm::vec3 origin;
        glm::vec3 extents;
    };

    struct BoundingSphere
    {
        glm::vec3 center;
        float radius;
    };

    //Passed to a uniform buffer descriptor set once per frame
    struct alignas(16)SceneData
    {
        glm::vec4 sunlightColor;
        glm::vec4 sunlightDirection;
        glm::vec4 ambientColor;

        //These will save view and clip coordinates from the camera to pass them to the shaders
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::mat4 projectionViewMatrix;

        //The scene data will be used to pass the vertex buffer address, this will probably need to be changed later
        VkDeviceAddress vertexBufferAddress; 
        //The scene data will be used to pass the material constants buffer address, this will probably need to be changed later
        VkDeviceAddress materialConstantsBufferAddress;
        //The scene data will be used to pass the indirect draw buffer address, this will probably need to be changed later
        VkDeviceAddress indirectBufferAddress;
        //The scene data will be used to pass the frustum collision buffer address, this will probably need to be changed later
        VkDeviceAddress frustumCollisionBufferAddress;
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
        //Used to draw each vertex of a surface(will be passed to vkCmdDrawIndexed or to an indirect command buffer)
        uint32_t firstIndex;
        uint32_t indexCount;

        MaterialInstance* pMaterial;

        OrientedBoundingBox obb;
        BoundingSphere sphereCollision;
    };

    struct MeshAsset
    {
        std::vector<GeoSurface> surfaces;
        std::string assetName;
    };

    //For every draw call in the non indirect pipeline, this will pass some per surface data
    struct alignas(16) DrawDataPushConstant
    {
        //The transform of a mesh, will transform every vertex to world coordinates
        glm::mat4 modelMatrix;
        //The material index which will indexes into the material constants used for a specific surface
        uint32_t materialIndex;
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

    //Used to write the data of textures used by materials into the uniform buffer arrays
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
    };

    //Holds material data, for the different types of materials that will be used by the renderer
    struct GlobalMaterialData
    {
        MaterialPipeline opaquePipeline;
        MaterialPipeline transparentPipeline;

        //The layout of the descriptor set used for all materials
        VkDescriptorSetLayout mainMaterialDescriptorSetLayout;
        //Used to allocate the material descriptor set after loading all scenes and assets
        DescriptorAllocator descriptorAllocator;
        //The descriptor set that will be bound every frame before drawing
        VkDescriptorSet mainMaterialDescriptorSet;

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

        //Saves the oriented bounding box of the surface to do frustum culling
        OrientedBoundingBox obb;

        //If frustum culling is done using a boudning sphere, this is used
        BoundingSphere sphereCollision;

        //Once frustum culling is complete every render object will have this set to false or true which will dictate if they get rendered or not
        bool bVisible = true;

        //When being added to the draw context each object has its worldTransorm decomposed to get the scale, position and rotation of the object
        glm::vec3 position;
        float scale;
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

    struct alignas(16) FrustumCollisionData
    {
        #if BLITZEN_FRUSTUM_CULLING_BOUNDING_SPHERE
            glm::vec3 center;
            float radius;
        #else
            glm::vec3 origin;
            glm::vec3 extents;
        #endif
    };

    struct DrawContext
    {
        std::vector<RenderObject> opaqueRenderObjects;

        //If Blitzen uses indirect commands, the draw context will include an array of draw indirect data
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            std::vector<DrawIndirectData> indirectDrawData;
            std::vector<FrustumCollisionData> surfaceFrustumCollisions;
        #endif
    };

    struct VulkanStats
    {
        #if BLITZEN_START_VULKAN_WITH_INDIRECT
            bool drawIndirectMode = true;
        #else 
            const bool drawIndirectMode = false;
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

        //Holds the uniform buffer used for material constants
        AllocatedBuffer m_materialDataBuffer;

        VulkanRenderer* m_pRenderer;

        void AddToDrawContext(glm::mat4& topMatrix, DrawContext& drawContex);

        void ClearAll();
    };

    //Gets generic data from the transform of an object that will be used for culling or other operations
    void DecomposeTransform(glm::vec3& translation, glm::vec4& rotation, glm::vec3& scale, const glm::mat4& transform);
}