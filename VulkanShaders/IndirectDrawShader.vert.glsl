#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include "GlobalShaderData.h"

//The fragment shader needs to be provided with the right material(what a strange way to phrase it, I'll leave it here)
layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUvMap;
layout(location = 2) out flat uint outMaterialIndex;

void main()
{
    Vertex currentVertex = sceneData.vertexBuffer.vertices[gl_VertexIndex];
    uint drawIndex = sceneData.indirectDataBuffer.draws[gl_DrawIDARB].objectId;
    RenderObject currentRender = sceneData.renderObjectBuffer.objects[gl_DrawIDARB];

    //Write the position of the vertex that is being processed to gl_Position
    gl_Position = sceneData.projectionViewMatrix * currentRender.modelMatrix * vec4(currentVertex.position, 1.0f);
    
    //Pass the color to the shader as the color factor of the material multiplied by the color of the vertex
    outColor = sceneData.materialConstantsBuffer.materialConstants[currentRender.materialIndex].
    colorFactor.xyz * currentVertex.color.xyz;
 
    outUvMap = vec2(currentVertex.uvMapX, currentVertex.uvMapY);
    
    outMaterialIndex = currentRender.materialIndex;
}
