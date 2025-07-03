#version 460 core

#extension GL_GOOGLE_include_directive : require

layout(set = 0, binding = 0) uniform  ShadowData{   
	mat4 shadowMatrices[4];
} shadowData;


layout (triangles, invocations = 4) in;
layout (triangle_strip, max_vertices = 3) out;

invariant gl_Position;
void main(void)
{
	for(int i = 0; i < gl_in.length(); ++i)
	{
		gl_Position = shadowData.shadowMatrices[gl_InvocationID] * gl_in[i].gl_Position.xyzw;
		gl_Layer = gl_InvocationID;
		EmitVertex();
	}
	EndPrimitive();
}  
