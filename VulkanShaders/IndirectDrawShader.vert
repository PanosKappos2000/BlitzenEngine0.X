#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters : require

//The draw indirect buffer, is used to also pass the world matrix of each object, to it needs to be access inside the shader
struct DrawIndirectData
{
    mat4 worldMatrix;

    //The VkDrawIndexedIndirectCommands are not needed inside the shader, so a placeholder value is put in its place
    float indirectCommands[5];
};

layout(buffer_reference, std430) readonly buffer IndirectDataBuffer
{
    DrawIndirectData indirectDraws[];
};

struct Vertex
{
    vec3 position;
    float uvMapX;
    vec3 normal;
    float uvMapY;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

#include "IndirectGlobalData.glsl"

layout(location = 0) out vec3 outColor;

void main()
{
    Vertex currentVertex = sceneData.vertexBuffer.vertices[gl_VertexIndex];
    mat4 currentWorldMatrix = sceneData.indirectDataBuffer.indirectDraws[gl_DrawIDARB].worldMatrix;

    gl_Position = sceneData.projectionViewMatrix * currentWorldMatrix * vec4(currentVertex.position, 1.0f);
    outColor = currentVertex.color.xyz; 

}