//The draw indirect buffer, is also used to pass some constant per surface data
struct IndirectDrawData
{
    mat4 worldMatrix;
    uint materialIndex;

    //The VkDrawIndexedIndirectCommands are not needed inside the shader, so a placeholder value is put in its place
    float indirectCommands[5];
};

layout(buffer_reference, std430) readonly buffer IndirectDataBuffer
{
    IndirectDrawData indirectDraws[];
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

struct MaterialConstants
{
    vec4 colorFactor;
    vec4 metalRoughFactor;
};

layout(buffer_reference, std430) readonly buffer MaterialConstantsBuffer
{
    MaterialConstants materialConstants[];
};

struct FrustumCollisionData
{
    vec3 center;
    float radius;
};

layout(buffer_reference, std430) readonly buffer FrustumCollisionDataBuffer
{
    FrustumCollisionData frustumCollisions[];
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
    VertexBuffer vertexBuffer;
    //The scene data will be used to pass the material constants buffer address, this will probably need to be changed later
    MaterialConstantsBuffer materialConstantsBuffer;
    //The scene data will be used to pass the indirect buffer address, this will probably need to be changed later
    IndirectDataBuffer indirectDataBuffer;
    //The scene data will be used to pass the frustum collision buffer address, this will probably need to be changed later
    FrustumCollisionDataBuffer frustCollisionDataBuffer;

}sceneData;