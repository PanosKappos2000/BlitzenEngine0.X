#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 uvMap;
layout(location = 2) in flat uint materialIndex;
layout(location = 0) out vec4 finalColor;

layout(set = 1, binding = 0) uniform sampler2D baseColorTextures[];
layout(set = 1, binding = 1) uniform sampler2D metalRoughTextures[];

void main()
{
    vec3 color = fragColor * texture(baseColorTextures[materialIndex],uvMap).xyz;
    finalColor = vec4(color, 1.0f);
}