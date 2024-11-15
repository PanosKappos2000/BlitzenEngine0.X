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
}sceneData;