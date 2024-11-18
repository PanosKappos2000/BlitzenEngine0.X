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

struct MaterialConstants
{
    vec4 colorFactor;
    vec4 metalRoughFactor;
};

layout(buffer_reference, std430) readonly buffer MaterialConstantsBuffer
{
    MaterialConstants materialConstants[];
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
}sceneData;