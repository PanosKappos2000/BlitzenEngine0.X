#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

struct Vertex
{
    vec3 position;
    float uvMapX;
    vec3 normal;
    float uvMapY;
    vec4 color;
};

layout(buffer_reference, std430)readonly buffer VertexBuffer
{
    Vertex vertices[];
};

#include "InputStructures.glsl"

layout(push_constant) uniform constants
{
    mat4 model;
}PushConstants;

layout(location = 0) out vec3 outColor;

void main()
{
    Vertex currentVertex = sceneData.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = sceneData.projectionViewMatrix * PushConstants.model * vec4(currentVertex.position, 1.0f);
    outColor = currentVertex.color.xyz;
}