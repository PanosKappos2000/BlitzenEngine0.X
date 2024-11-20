#version 450

#extension GL_EXT_nonuniform_qualifier : require

//The vertex color passed from the vertex shader
layout(location = 0) in vec3 fragColor;
//Used to properly bind the texture to the fragment and passed from the vertex shader
layout(location = 1) in vec2 uvMap;
//This is passed from the vertex shader so that the fragment shader can index into the texture array
layout(location = 2) in /*the flat keyword is required to pass uint to the shaders*/flat uint materialIndex;
//Output of the fragment shader
layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform sampler2D baseColorTextures[];
layout(set = 1, binding = 1) uniform sampler2D metalRoughTextures[];

void main()
{
    vec3 color = fragColor * texture(baseColorTextures[materialIndex],uvMap).xyz;
    finalColor = vec4(color, 1.0f);
}