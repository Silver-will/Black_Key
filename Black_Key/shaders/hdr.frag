#version 460 core

#extension GL_GOOGLE_include_directive : require
#include "helper_functions.glsl"
#include "fxaa.glsl"
layout (set = 0, binding = 1) uniform sampler2D debugImage;

layout(location = 0)in vec2 TexCoords;
layout (location = 1)flat in uint debug_texture;

layout (location = 0)out vec4 FragColor;

void main()
{
	if(debug_texture == 0)
	{
		FragColor = texture(HDRImage,TexCoords);
		FragColor = FXAA(TexCoords);
		vec3 color = neutral(FragColor.rgb);
		// gamma correct
		FragColor = vec4(pow(color, vec3(1.0/2.2)),1.0);
	}
	else
	{
		float r = texture(debugImage, TexCoords).r;
		FragColor = vec4(r,r,r,r);
	}
}