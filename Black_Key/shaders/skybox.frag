#version 450

layout (set = 0, binding = 1) uniform samplerCube samplerCubeMap;
layout(early_fragment_tests) in;

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	outFragColor = texture(samplerCubeMap, inUVW);
	//outFragColor = vec4(1,0,0,0);
}