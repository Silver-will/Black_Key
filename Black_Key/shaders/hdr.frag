#version 460 core

#extension GL_GOOGLE_include_directive : require
#include "helper_functions.glsl"
#include "fxaa.glsl"
layout (set = 0, binding = 1) uniform sampler2D debugImage;
layout (set = 0, binding = 2) uniform sampler2D bloomImage;

layout(location = 0)in vec2 TexCoords;
layout (location = 0)out vec4 FragColor;

layout( push_constant ) uniform constants
{
	uint use_fxaa;
	uint use_smaa;
	uint debug_texture;
	float bloom_strength;
} PushConstants;

void main()
{
	if(PushConstants.debug_texture == 0)
	{
		vec4 HDRColor = texture(HDRImage,TexCoords);
		vec4 bloomColor = texture(bloomImage, TexCoords);
		
		//Anti aliasing
		if(PushConstants.use_fxaa == 1) HDRColor = FXAA(TexCoords);

		vec3 color = mix(HDRColor, bloomColor, PushConstants.bloom_strength).rgb;
		color = neutral(color);
	
	// gamma correct
		FragColor = vec4(pow(color, vec3(1.0/2.2)),1.0);
	}
	else
	{
		FragColor = texture(bloomImage, TexCoords);
	}
}