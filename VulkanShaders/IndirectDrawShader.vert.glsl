#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include "GlobalShaderData.h"

layout(location = 0) out vec3 outColor;

void main()
{
    Vertex currentVertex = sceneData.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = sceneData.projectionViewMatrix * sceneData.indirectDataBuffer.indirectDraws[gl_DrawIDARB].worldMatrix * vec4(currentVertex.position, 1.0f);
    outColor = sceneData.materialConstantsBuffer.materialConstants[16].colorFactor.xyz * currentVertex.color.xyz; 

}