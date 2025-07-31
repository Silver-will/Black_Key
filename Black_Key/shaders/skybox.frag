#version 460 core

layout(early_fragment_tests) in;
layout (set = 0, binding = 1) uniform samplerCube samplerCubeMap;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 FragColor;

void main() 
{
	vec3 texCoords;
	texCoords = vec3(inUVW.r,inUVW.g, inUVW.b);
	FragColor = texture(samplerCubeMap, texCoords);
}