//The draw indirect buffer, is also used to pass some constant per surface data
struct IndirectDrawData
{
    uint objectId;
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;

    uint padding[2];
};

layout(buffer_reference, std430) readonly buffer IndirectDataBuffer
{
    IndirectDrawData draws[];
};

#ifdef COMPUTE_SHADER
    layout(buffer_reference, std430) writeonly buffer FinalIndirectBuffer
    {
        IndirectDrawData draws[];
    };
#else
    layout(buffer_reference, std430) readonly buffer FinalIndirectBuffer
    {
        IndirectDrawData draws[];
    };
#endif

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

struct RenderObject 
{
    mat4 modelMatrix;

    vec3 center;
    float radius;

    uint materialIndex;
};

layout(buffer_reference, std430) readonly buffer RenderObjectBuffer
{
    RenderObject objects[];
};

layout(set = 0, binding = 0) uniform SceneData
{
    vec4 frustumData[6];
    vec4 sunlightColor;
    vec4 sunlightDirection;
    vec4 ambientColor;
    mat4 viewMatrix;
    mat4 projectionViewMatrix;

    //The scene data will be used to pass the vertex buffer address, this will probably need to be changed later
    VertexBuffer vertexBuffer;
    //The scene data will be used to pass the material constants buffer address, this will probably need to be changed later
    MaterialConstantsBuffer materialConstantsBuffer;
    //The scene data will be used to pass the indirect buffer address, this will probably need to be changed later
    IndirectDataBuffer indirectDataBuffer;
    FinalIndirectBuffer finalIndirect;
    //The scene data will be used to pass the frustum collision buffer address, this will probably need to be changed later
    RenderObjectBuffer renderObjectBuffer;

}sceneData;
