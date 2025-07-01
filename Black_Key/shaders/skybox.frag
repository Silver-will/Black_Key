#version 460 core

layout(early_fragment_tests) in;
layout (set = 0, binding = 1) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 FragColor;

void main() 
{
	FragColor = texture(samplerCubeMap, inUVW);
}