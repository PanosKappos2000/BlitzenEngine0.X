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
            for(GeoSurface& surface : pMesh->surfaces)
            {
                //Each surface on a mesh adds a render object on the draw context
                drawContext.opaqueRenderObjects.push_back(RenderObject());
                RenderObject& newObject = drawContext.opaqueRenderObjects.back();
                //Pass every surface constant to the object, so that it can be bound when drawing
                newObject.firstIndex = surface.firstIndex;
                newObject.indexCount = surface.indexCount;
                newObject.pMaterial = surface.pMaterial;
                //The object will also need the mesh's/node's final matrix
                newObject.modelMatrix = finalMatrix;
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