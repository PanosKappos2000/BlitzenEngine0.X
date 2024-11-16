#include "vulkanData.h"
//Included in the source and forward declared in the header to avoid circular dependency
#include "vulkanRenderer.h"

namespace BlitzenVulkan
{
    void Node::UpdateTransform(const glm::mat4& topMatrix)
    {
        //The parent's matrix is used to get the world transform
        worldTransform = topMatrix * localTransform;
        for(Node* child : m_children)
        {
            child->UpdateTransform(worldTransform);
        }
    }

    void Node::AddToDrawContext(glm::mat4& topMatrix, DrawContext& drawContext)
    {
        //Most of the work is done when the node is a mesh node
        if(pMesh)
        {
            //A mesh node's transform goes through one final modification, where it is multiplied by the scene's transform
            glm::mat4 finalMatrix = topMatrix * worldTransform;

            //All the surfaces of the mesh will be added to the draw context, so it is resized in advance
            size_t startIndex = drawContext.opaqueRenderObjects.size();
            drawContext.opaqueRenderObjects.resize(drawContext.opaqueRenderObjects.size() + pMesh->surfaces.size());
            //If vulkan is going to need draw indirect during runtime, for each surface in the mesh there needs to be a draw indirect command
            #if BLITZEN_START_VULKAN_WITH_INDIRECT
                drawContext.indirectData.resize(drawContext.opaqueRenderObjects.size());
            #endif
            for(size_t i = startIndex; i < drawContext.opaqueRenderObjects.size(); ++i)
            {
                //Getting the current surface from the mesh and the current render object from the ones that were newly allocated
                GeoSurface& currentSurface = pMesh->surfaces[i - startIndex];
                RenderObject& newObject = drawContext.opaqueRenderObjects[i];
                //Pass every surface constant to the object, so that it can be bound when drawing
                newObject.firstIndex = currentSurface.firstIndex;
                newObject.indexCount = currentSurface.indexCount;
                newObject.pMaterial = currentSurface.pMaterial;
                //The object will also need the mesh's/node's final matrix
                newObject.modelMatrix = finalMatrix;

                //Setting up for draw indirect if it needs to be available
                #if BLITZEN_START_VULKAN_WITH_INDIRECT
                    VkDrawIndexedIndirectCommand& currentDraw = drawContext.indirectData[i].indirectDraws;
                    currentDraw.firstIndex = currentSurface.firstIndex;
                    currentDraw.instanceCount = 1;
                    currentDraw.indexCount = currentSurface.indexCount;
                    currentDraw.firstInstance = 0;
                    currentDraw.vertexOffset = 0;

                    //Give the object's world matrix as well, so that it is passed to the GPU
                    drawContext.indirectData[i].worldMatrix = finalMatrix;
                #endif
            }
        }
        //Recurse down to update the next mesh node that is found
        for(Node* child : m_children)
        {
            child->AddToDrawContext(topMatrix, drawContext);
        }
    }

    void LoadedScene::AddToDrawContext(glm::mat4& topMatrix, DrawContext& drawContext)
    {
        //Iterates through the parent nodes to update their children based on the hierarchy
        for(Node* pNode : m_pureParentNodes)
        {
            pNode->AddToDrawContext(topMatrix, drawContext);
        }
    }

    void LoadedScene::ClearAll()
    {
        m_descriptorAllocator.CleanupResources();

        vmaDestroyBuffer(m_pRenderer->m_allocator, m_materialDataBuffer.buffer, m_materialDataBuffer.allocation);

        for(auto& [name, texture] : m_textures)
        {
            texture.CleanupResources(m_pRenderer->m_device, m_pRenderer->m_allocator);
        }

        for(size_t i = 0; i < m_samplers.size(); ++i)
        {
            vkDestroySampler(m_pRenderer->m_device, m_samplers[i], nullptr);
        }
    }
}