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
	
	/*gl_Position = gl_in[0].gl_Position.xyzw;
	EmitVertex();

	gl_Position = gl_in[1].gl_Position.xyzw;
	EmitVertex();
	
	gl_Position = gl_in[2].gl_Position.xyzw;
	EmitVertex();
	
	EndPrimitive();
	*/
}  
