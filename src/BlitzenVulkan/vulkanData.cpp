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

            glm::vec3 translation;
            glm::vec3 scale;
            glm::vec4 rotation;
            DecomposeTransform(translation, rotation, scale, finalMatrix);

            //All the surfaces of the mesh will be added to the draw context, so it is resized in advance
            size_t startIndex = drawContext.opaqueRenderObjects.size();
            drawContext.opaqueRenderObjects.resize(drawContext.opaqueRenderObjects.size() + pMesh->surfaces.size());
            //If vulkan is going to need draw indirect during runtime, for each surface in the mesh there needs to be a draw indirect command
            #if BLITZEN_START_VULKAN_WITH_INDIRECT
                drawContext.indirectDrawData.resize(drawContext.opaqueRenderObjects.size());
                drawContext.renderObjects.resize(drawContext.opaqueRenderObjects.size());
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
                newObject.center = currentSurface.center;
                newObject.radius = currentSurface.radius;

                //Setting up for draw indirect if it needs to be available
                #if BLITZEN_START_VULKAN_WITH_INDIRECT
                    VkDrawIndexedIndirectCommand& currentDraw = drawContext.indirectDrawData[i].indirectDraws;
                    currentDraw.firstIndex = currentSurface.firstIndex;
                    currentDraw.instanceCount = 1;
                    currentDraw.indexCount = currentSurface.indexCount;
                    currentDraw.firstInstance = 0;
                    currentDraw.vertexOffset = 0;

                    //Update object data so that it can be drawn properly from with the shader
                    drawContext.indirectDrawData[i].worldMatrix = finalMatrix;
                    drawContext.indirectDrawData[i].materialIndex = currentSurface.pMaterial->materialIndex;
                    //Giving the bounding object to the surface frustum collision so that it can be passed to a draw indirect buffer
                    drawContext.renderObjects[i].center = currentSurface.center;
                    drawContext.renderObjects[i].radius = currentSurface.radius;
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

    void DecomposeTransform(glm::vec3& translation, glm::vec4& rotation, glm::vec3& scale, const glm::mat4& transform)
    {
        translation[0] = transform[3][0];
	    translation[1] = transform[3][1];
	    translation[2] = transform[3][2];

	    //Compute determinant to determine handedness
	    float det = transform[0][0] * (transform[1][1] * transform[2][2] - transform[2][1] * transform[1][2]) -
	    transform[0][1] * (transform[1][0] * transform[2][2] - transform[1][2] * transform[2][0]) +
	    transform[0][2] * (transform[1][0] * transform[2][1] - transform[1][1] * transform[2][0]);
	    float sign = (det < 0.f) ? -1.f : 1.f;

	    // recover scale from axis lengths
	    scale[0] = sqrtf(transform[0][0] * transform[0][0] + transform[0][1] * transform[0][1] + transform[0][2] * transform[0][2]) * sign;
	    scale[1] = sqrtf(transform[1][0] * transform[1][0] + transform[1][1] * transform[1][1] + transform[1][2] * transform[1][2]) * sign;
	    scale[2] = sqrtf(transform[2][0] * transform[2][0] + transform[2][1] * transform[2][1] + transform[2][2] * transform[2][2]) * sign;

        // normalize axes to get a pure rotation matrix
	    float rsx = (scale[0] == 0.f) ? 0.f : 1.f / scale[0];
	    float rsy = (scale[1] == 0.f) ? 0.f : 1.f / scale[1];
	    float rsz = (scale[2] == 0.f) ? 0.f : 1.f / scale[2];

        float r00 = transform[0][0] * rsx, r10 = transform[1][0] * rsy, r20 = transform[2][0] * rsz;
	    float r01 = transform[0][1] * rsx, r11 = transform[1][1] * rsy, r21 = transform[2][1] * rsz;
	    float r02 = transform[0][2] * rsx, r12 = transform[1][2] * rsy, r22 = transform[2][2] * rsz;

        int qc = r22 < 0 ? (r00 > r11 ? 0 : 1) : (r00 < -r11 ? 2 : 3);
	    float qs1 = qc & 2 ? -1.f : 1.f;
	    float qs2 = qc & 1 ? -1.f : 1.f;
	    float qs3 = (qc - 1) & 2 ? -1.f : 1.f;

        float qt = 1.f - qs3 * r00 - qs2 * r11 - qs1 * r22;
	    float qs = 0.5f / sqrtf(qt);

	    rotation[qc ^ 0] = qs * qt;
	    rotation[qc ^ 1] = qs * (r01 + qs1 * r10);
	    rotation[qc ^ 2] = qs * (r20 + qs2 * r02);
	    rotation[qc ^ 3] = qs * (r12 + qs3 * r21);
    }
}