#version 450 core

layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 3) out;

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#include "input_structures.glsl"


void main(void)
{          
	for (int i = 0; i < 3; ++i)
	{
		gl_Position = sceneData.lightMatrices[gl_InvocationID] * gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		EmitVertex();
	}
	EndPrimitive();
}  
