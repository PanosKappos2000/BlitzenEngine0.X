#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "GlobalShaderData.h"

layout(push_constant) uniform constants
{
    mat4 model;
    uint materialIndex;
}PushConstants;

layout(location = 0) out vec3 outColor;

void main()
{
    Vertex currentVertex = sceneData.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = sceneData.projectionViewMatrix * PushConstants.model * vec4(currentVertex.position, 1.0f);
    outColor = sceneData.materialConstantsBuffer.materialConstants[PushConstants.materialIndex].colorFactor.xyz * currentVertex.color.xyz;
}