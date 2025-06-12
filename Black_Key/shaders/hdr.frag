#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "helper_functions.glsl"
#include "fxaa.glsl"

layout(location = 0)in vec2 TexCoords;
layout (location = 0)out vec4 FragColor;

void main()
{
	FragColor = texture(HDRImage,TexCoords);
	FragColor = FXAA(TexCoords);
	
	vec3 color = neutral(FragColor.rgb);
	// gamma correct
    FragColor = vec4(pow(color, vec3(1.0/2.2)),1.0);
   
}