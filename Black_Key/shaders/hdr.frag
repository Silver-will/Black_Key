#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "helper_functions.glsl"

layout(location = 0)in vec2 TexCoords;
layout (set = 0, binding = 0) uniform sampler2D HDRImage;

layout (location = 0)out vec4 FragColor;

void main()
{
	FragColor = texture(HDRImage,TexCoords);
	
	vec3 color = uchimura(FragColor.rgb);
	// gamma correct
    FragColor = vec4(pow(color, vec3(1.0/2.2)),1.0);
   
}