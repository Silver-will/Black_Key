#version 450 core

layout(location = 0)in vec2 TexCoords;
layout (set = 0, binding = 0) uniform sampler2D HDRImage;

layout (location = 0)out vec4 FragColor;

void main()
{
	FragColor = texture(HDRImage,TexCoords);
}