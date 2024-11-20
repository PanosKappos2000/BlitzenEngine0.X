#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

struct FrustumCollisionData
{
    vec3 center;
    float radius;
};

layout(buffer_reference, std430) readonly buffer FrustumCollisionDataBuffer
{
    FrustumCollisionData frustumCollisions[];
};

struct IndirectDrawData
{
    mat4 worldMatrix;
    uint materialIndex;

    //The VkDrawIndexedIndirectCommands are not needed inside the shader, so a placeholder value is put in its place
    float indirectCommands[5];
};

layout(buffer_reference, std430) writeonly buffer IndirectDataBuffer
{
    IndirectDrawData indirectDraws[];
};

layout(set = 0, binding = 0) uniform SceneData
{
    vec4 sunlightColor;
    vec4 sunlightDirection;
    vec4 ambientColor;
    mat4 viewMatrix;
    mat4 projectionMatrix;
    mat4 projectionViewMatrix;
    //The scene data will be used to pass the vertex buffer address, this will probably need to be changed later
    uint vertexBuffer;
    //The scene data will be used to pass the material constants buffer address, this will probably need to be changed later
    uint materialConstantsBuffer;
    //The scene data will be used to pass the indirect buffer address, this will probably need to be changed later
    IndirectDataBuffer indirectDataBuffer;
    //The scene data will be used to pass the frustum collision buffer address, this will probably need to be changed later
    FrustumCollisionDataBuffer frustCollisionDataBuffer;

}sceneData;

void main()
{
    
}