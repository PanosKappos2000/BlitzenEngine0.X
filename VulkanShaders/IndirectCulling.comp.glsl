#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#define COMPUTE_SHADER

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

#include "GlobalShaderData.h"

void main()
{
    uint objectIndex = gl_GlobalInvocationID.x;
    IndirectDrawData currentRead = sceneData.indirectDataBuffer.draws[objectIndex];
    RenderObject currentObject = sceneData.renderObjectBuffer.objects[objectIndex];

    vec3 center = (currentObject.modelMatrix * vec4(currentObject.center, 1)).xyz;
    center = (sceneData.viewMatrix * vec4(center, 1)).xyz;
    float radius = currentObject.radius;

    bool visible = true;
    for(uint i = 0; i < 6; ++i)
        visible = visible && dot(sceneData.frustumData[i], vec4(center, 1)) > -radius;

    sceneData.finalIndirect.draws[objectIndex].indexCount = currentRead.indexCount;
    sceneData.finalIndirect.draws[objectIndex].instanceCount = visible ? 1 : 0;
    sceneData.finalIndirect.draws[objectIndex].firstIndex = currentRead.firstIndex;
    sceneData.finalIndirect.draws[objectIndex].vertexOffset = 0;
    sceneData.finalIndirect.draws[objectIndex].firstInstance  = 0;

    
}