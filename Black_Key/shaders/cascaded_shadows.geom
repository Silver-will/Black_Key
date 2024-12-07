#version 450 core

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (triangles, invocations = 5) in;
layout (triangle_strip, max_vertices = 3) out;

void main(void)
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = sceneData.lightMatrices[gl_InvocationID] * gl_in[i].gl_Position.xyzw;
		gl_Layer = gl_InvocationID;
		EmitVertex();
	}
	EndPrimitive();
	
}  
